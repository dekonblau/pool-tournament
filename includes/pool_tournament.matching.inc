<?php
/**
 * @file
 * Player/tournament-parameter matching and Fargo eligibility.
 *
 * Backs two things: notifying players when a new tournament matches their
 * preferences, and (later) letting a TD check how many players match a
 * hypothetical set of parameters before actually creating a tournament.
 * Both need the same core comparison, so it's factored out here rather
 * than tied to a specific node or form.
 */

/**
 * Checks whether a player's preferences match a set of tournament
 * parameters.
 *
 * Same "unset = no restriction" semantics as the /tournaments listing
 * filter (pool_tournament_tournaments_page()): a dimension the player has
 * no preference for imposes no restriction and always matches. A
 * dimension with a preference set only matches if $params has a value for
 * that dimension AND it's one of the preferred IDs.
 *
 * @param array $prefs
 *   Player preferences as returned by
 *   _pool_tournament_player_preferences_load() -- keyed by preference_type
 *   (see _pool_tournament_preference_field_map()), each value an array of
 *   preferred entity IDs.
 * @param array $params
 *   Candidate tournament parameters, keyed the same way as $prefs, each
 *   value a single entity ID (or NULL/absent if that dimension isn't set
 *   for this candidate -- e.g. a hypothetical set of parameters that
 *   hasn't picked a venue yet).
 *
 * @return bool
 *   TRUE if every dimension the player has a preference for is satisfied.
 */
function pool_tournament_preferences_match(array $prefs, array $params) {
  foreach ($prefs as $ptype => $preferred_ids) {
    if (empty($preferred_ids)) {
      // No preference on this dimension -- doesn't restrict the match.
      continue;
    }
    if (empty($params[$ptype]) || !in_array($params[$ptype], $preferred_ids)) {
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Extracts a tournament node's parameters into the same shape
 * pool_tournament_preferences_match() expects for $params.
 *
 * @param object $node
 *   A tournament node, with Field API values attached (as they are on any
 *   normally-loaded node).
 *
 * @return array
 *   Keyed by preference_type, each value the node's field value as an int,
 *   or NULL if that field isn't set on this node.
 */
function pool_tournament_tournament_node_params($node) {
  $params = array();
  foreach (_pool_tournament_preference_field_map() as $ptype => $field_name) {
    $value = isset($node->{$field_name}[LANGUAGE_NONE][0]['value'])
      ? $node->{$field_name}[LANGUAGE_NONE][0]['value']
      : NULL;
    $params[$ptype] = ($value !== NULL && $value !== '') ? (int) $value : NULL;
  }
  return $params;
}

/**
 * Checks a player's Fargo eligibility against a rating ceiling, via a fresh
 * live lookup (never the player's stored rating -- it could be stale).
 *
 * The ceiling is a per-tournament value (see pool_tournament_tournament_
 * extra and pool_tournament_tournament_fargo_ceiling_validate() -- it's a
 * decision the venue makes fresh for each tournament, NOT a property of the
 * tournament_type entity, since the same fargo_limit(_handicapped) type
 * gets reused across many tournaments with different ceilings), so callers
 * pass it in directly rather than this function looking it up itself.
 *
 * @param object $player
 *   A player entity.
 * @param int|NULL $ceiling
 *   The tournament's Fargo rating ceiling, or NULL if none is set (e.g. the
 *   tournament isn't a Fargo-limited type).
 *
 * @return array|FALSE
 *   FALSE if the Fargo service itself is unreachable -- see
 *   _pool_tournament_fargo_request(); callers running a batch across many
 *   players must treat this as a hard-stop-the-whole-batch signal, not a
 *   single-player failure, since a down service can't verify anyone.
 *   Otherwise an array:
 *     - 'status': 'eligible', 'ineligible', or 'skip'. 'skip' covers both
 *       "no member_id on file" and "member_id on file, but Fargo has no
 *       record for it" -- either way eligibility can't be verified, so
 *       the player is neither auto-included nor auto-excluded.
 *     - 'rating': the fresh effectiveRating if found, else NULL.
 *     - 'ceiling': the ceiling passed in, normalized to int|NULL.
 */
function pool_tournament_fargo_eligibility($player, $ceiling) {
  $ceiling = ($ceiling !== NULL && $ceiling !== '') ? (int) $ceiling : NULL;

  module_load_include('inc', 'pool_tournament', 'includes/pool_tournament.fargo');
  $rating_raw = pool_tournament_fargo_current_rating($player);

  if ($rating_raw === FALSE) {
    // Service down -- hard stop, not a per-player skip.
    return FALSE;
  }

  $rating = ($rating_raw !== NULL) ? (int) $rating_raw : NULL;

  if ($rating === NULL || $ceiling === NULL) {
    // No member_id, Fargo has no record for it, or no ceiling configured --
    // can't verify either way.
    return array('status' => 'skip', 'rating' => $rating, 'ceiling' => $ceiling);
  }

  return array(
    'status'  => ($rating <= $ceiling) ? 'eligible' : 'ineligible',
    'rating'  => $rating,
    'ceiling' => $ceiling,
  );
}

/**
 * Notifies every player whose preferences match a newly-created tournament.
 * Called from pool_tournament_node_insert() -- never on update, so editing
 * an existing tournament doesn't re-notify anyone.
 *
 * For a fargo_limit(_handicapped) tournament type, players are additionally
 * filtered by a live eligibility check; a fresh rating obtained during that
 * check is written back to the player's durable fargo_rating column, since
 * it's already been fetched and matching players are exactly the ones worth
 * keeping current. Players who don't match aren't looked up at all -- no
 * Fargo API call is made on their behalf.
 *
 * @param object $node
 *   A newly-inserted tournament node, with its Field API values attached.
 *
 * @return array
 *   Counts for logging/testing: 'notified', 'ineligible', 'no_match', and
 *   'aborted' (TRUE if a down Fargo service cut the batch short -- see
 *   pool_tournament_fargo_eligibility()).
 */
function pool_tournament_notify_matching_players($node) {
  module_load_include('inc', 'pool_tournament', 'includes/pool_tournament.forms');

  $params = pool_tournament_tournament_node_params($node);

  // The ceiling lives per-tournament (pool_tournament_tournament_extra),
  // not on the tournament_type entity -- see pool_tournament_fargo_
  // eligibility()'s docblock. Its mere presence is the gate for whether to
  // even attempt a live Fargo check at all, since the node form only ever
  // persists a non-NULL ceiling for a Fargo-limited tournament_type bundle
  // (pool_tournament_tournament_fargo_ceiling_validate() clears it
  // otherwise) -- no need to separately load/check the tournament_type.
  $ceiling = db_select('pool_tournament_tournament_extra', 'e')
    ->fields('e', array('fargo_rating_ceiling'))
    ->condition('nid', $node->nid)
    ->execute()
    ->fetchField();
  $has_ceiling = ($ceiling !== FALSE && $ceiling !== NULL);

  $summary = array('notified' => 0, 'ineligible' => 0, 'no_match' => 0, 'aborted' => FALSE);

  $player_ids = db_select('pool_tournament_player', 'p')
    ->fields('p', array('player_id'))
    ->execute()
    ->fetchCol();

  foreach ($player_ids as $player_id) {
    $player = pool_tournament_player_load($player_id);
    if (!$player) {
      continue;
    }

    $prefs = _pool_tournament_player_preferences_load($player_id);
    if (!pool_tournament_preferences_match($prefs, $params)) {
      $summary['no_match']++;
      continue;
    }

    $rating = NULL;
    if ($has_ceiling) {
      $eligibility = pool_tournament_fargo_eligibility($player, $ceiling);
      if ($eligibility === FALSE) {
        // Service down -- can't verify anyone else either. Stop rather
        // than silently treating the rest of the batch as ineligible.
        watchdog('pool_tournament',
          'Tournament @nid notification batch aborted: Fargo service unreachable.',
          array('@nid' => $node->nid), WATCHDOG_ERROR);
        $summary['aborted'] = TRUE;
        return $summary;
      }
      if ($eligibility['status'] === 'ineligible') {
        $summary['ineligible']++;
        continue;
      }
      // 'eligible' or 'skip' (can't verify) both proceed to notification --
      // skip means neither auto-included nor auto-excluded, and excluding
      // someone from a notification on unverifiable data is the wrong
      // direction to err in.
      $rating = $eligibility['rating'];
    }

    if ($rating !== NULL && (string) $rating !== (string) $player->fargo_rating) {
      $player->fargo_rating = $rating;
      $player->save();
    }

    _pool_tournament_notify_player($player, $node);
    $summary['notified']++;
  }

  return $summary;
}

/**
 * Sends a single player's new-tournament-match notification.
 *
 * Stub -- actual email content/template is deferred until the player and
 * tournament logic it depends on is working end to end. For now this just
 * logs what would have been sent, so the matching/eligibility pipeline
 * feeding it can be exercised and verified on its own.
 */
function _pool_tournament_notify_player($player, $node) {
  watchdog('pool_tournament',
    'Would notify player @label (id @pid) about new tournament "@title" (nid @nid).',
    array(
      '@label' => $player->label,
      '@pid'   => $player->id(),
      '@title' => $node->title,
      '@nid'   => $node->nid,
    ));
}

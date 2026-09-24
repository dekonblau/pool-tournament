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
 * Checks a player's Fargo eligibility for a fargo_limit(_handicapped)
 * tournament type, via a fresh live lookup (never the player's stored
 * rating -- it could be stale).
 *
 * @param object $player
 *   A player entity.
 * @param object $tournament_type
 *   A tournament_type entity; expected bundle fargo_limit or
 *   fargo_limit_handicapped, but works off whatever ceiling (if any) is
 *   actually stored for it rather than checking the bundle itself.
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
 *     - 'ceiling': the tournament type's stored rating ceiling, or NULL
 *       if none is set.
 */
function pool_tournament_fargo_eligibility($player, $tournament_type) {
  $ceiling = db_select('pool_tournament_tournament_type_fargo_limit', 'f')
    ->fields('f', array('fargo_rating_ceiling'))
    ->condition('tournament_type_id', $tournament_type->id())
    ->execute()
    ->fetchField();
  $ceiling = ($ceiling !== FALSE && $ceiling !== NULL) ? (int) $ceiling : NULL;

  if (empty($player->member_id)) {
    return array('status' => 'skip', 'rating' => NULL, 'ceiling' => $ceiling);
  }

  module_load_include('inc', 'pool_tournament', 'includes/pool_tournament.fargo');
  $results = pool_tournament_fargo_lookup_all($player->member_id);

  if ($results === FALSE) {
    // Service down -- hard stop, not a per-player skip.
    return FALSE;
  }

  if (empty($results)) {
    // A member_id is on file, but Fargo has no record for it -- can't verify.
    return array('status' => 'skip', 'rating' => NULL, 'ceiling' => $ceiling);
  }

  $mapped = pool_tournament_fargo_map_to_player($results[0]);
  $rating = isset($mapped['fargo_rating']) && $mapped['fargo_rating'] !== NULL
    ? (int) $mapped['fargo_rating'] : NULL;

  if ($rating === NULL || $ceiling === NULL) {
    return array('status' => 'skip', 'rating' => $rating, 'ceiling' => $ceiling);
  }

  return array(
    'status'  => ($rating <= $ceiling) ? 'eligible' : 'ineligible',
    'rating'  => $rating,
    'ceiling' => $ceiling,
  );
}

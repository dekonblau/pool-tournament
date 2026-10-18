<?php
/**
 * @file
 * Tournament roster: player registrations, each with a point-in-time
 * Fargo rating snapshot.
 *
 * A registration is a weak entity -- no identity of its own, no bundles,
 * just a fact keyed by (nid, player_id): this player signed up for this
 * tournament at this time, with their Fargo rating as of that moment. The
 * snapshot is captured once and never updated afterward even if the
 * player's live rating later changes -- it's a historical record (e.g. for
 * auditing a fargo_limit tournament's eligibility after the fact), not a
 * live value. Use pool_tournament_fargo_eligibility() for live checks.
 */

/**
 * Registers a player for a tournament, capturing a fresh Fargo rating
 * snapshot. Re-registering (e.g. after withdrawing) refreshes both the
 * snapshot and the timestamp.
 *
 * @param int $nid
 *   Tournament node ID.
 * @param int $player_id
 *   Player entity ID.
 *
 * @return object|FALSE
 *   The registration record (see pool_tournament_registration_load()), or
 *   FALSE if $nid isn't a tournament node or $player_id doesn't load.
 */
function pool_tournament_register_player($nid, $player_id) {
  $node = node_load($nid);
  if (!$node || $node->type !== 'tournament') {
    return FALSE;
  }
  $player = pool_tournament_player_load($player_id);
  if (!$player) {
    return FALSE;
  }

  module_load_include('inc', 'pool_tournament', 'includes/pool_tournament.fargo');
  $rating = pool_tournament_fargo_current_rating($player);
  // Best-effort snapshot -- registration succeeds either way; a down
  // Fargo service or a player with no member_id on file just means an
  // empty snapshot, not a failed registration.
  $snapshot = ($rating === FALSE) ? NULL : $rating;

  db_merge('pool_tournament_registration')
    ->key(array('nid' => $nid, 'player_id' => $player_id))
    ->fields(array(
      'fargo_rating_snapshot' => $snapshot,
      'registered' => REQUEST_TIME,
    ))
    ->execute();

  return pool_tournament_registration_load($nid, $player_id);
}

/**
 * Removes a player's registration for a tournament.
 *
 * @return bool
 *   TRUE if a registration existed and was removed, FALSE if there was
 *   nothing to remove.
 */
function pool_tournament_unregister_player($nid, $player_id) {
  $deleted = db_delete('pool_tournament_registration')
    ->condition('nid', $nid)
    ->condition('player_id', $player_id)
    ->execute();
  return (bool) $deleted;
}

/**
 * Loads a single registration record.
 *
 * @return object|FALSE
 */
function pool_tournament_registration_load($nid, $player_id) {
  return db_select('pool_tournament_registration', 'r')
    ->fields('r')
    ->condition('nid', $nid)
    ->condition('player_id', $player_id)
    ->execute()
    ->fetchObject();
}

/**
 * Loads every registration for a tournament, earliest first.
 *
 * @param int $nid
 *   Tournament node ID.
 *
 * @return object[]
 *   Registration records keyed by player_id.
 */
function pool_tournament_tournament_registrations($nid) {
  return db_select('pool_tournament_registration', 'r')
    ->fields('r')
    ->condition('nid', $nid)
    ->orderBy('registered', 'ASC')
    ->execute()
    ->fetchAllAssoc('player_id');
}

// ---------------------------------------------------------------------------
// Access control
// ---------------------------------------------------------------------------

/**
 * Whether a tournament is "in play" -- once TRUE, self-service
 * registration/withdrawal is blocked; only the TD/an admin may still
 * modify the roster (see pool_tournament_tournament_td_access()).
 */
function pool_tournament_tournament_in_play($nid) {
  $in_play = db_select('pool_tournament_tournament_extra', 'e')
    ->fields('e', array('in_play'))
    ->condition('nid', $nid)
    ->execute()
    ->fetchField();
  return $in_play ? TRUE : FALSE;
}

/**
 * Whether the current user is this tournament's assigned TD, or a site
 * admin acting as one. Gates both the "Tournament in Play" toggle and
 * roster management, regardless of in_play state -- the TD retains full
 * control throughout, only self-service is ever blocked.
 */
function pool_tournament_tournament_td_access($node) {
  if (!is_object($node) || $node->type !== 'tournament') {
    return FALSE;
  }
  if (user_access('administer pool tournament')) {
    return TRUE;
  }
  global $user;
  $td_uid = db_select('pool_tournament_tournament_extra', 'e')
    ->fields('e', array('td_uid'))
    ->condition('nid', $node->nid)
    ->execute()
    ->fetchField();
  return $td_uid && ((int) $td_uid === (int) $user->uid);
}

/**
 * Whether the current user may self-register/withdraw: must be a
 * tournament node, registration must actually be open (a trial-balloon
 * tournament with "Do not open registration yet" checked doesn't accept
 * self-registration even though it isn't "in play" either), not yet in
 * play, and the user must have a linked player record.
 */
function pool_tournament_registration_self_access($node) {
  if (!is_object($node) || $node->type !== 'tournament') {
    return FALSE;
  }
  if (!_pool_tournament_registration_is_open($node)) {
    return FALSE;
  }
  if (pool_tournament_tournament_in_play($node->nid)) {
    return FALSE;
  }
  global $user;
  return (bool) pool_tournament_player_for_user($user->uid);
}

// ---------------------------------------------------------------------------
// Self-service register/withdraw
// ---------------------------------------------------------------------------

/**
 * Single-button register/withdraw toggle for the current user's own
 * player record -- no confirm step, since this is low-stakes and freely
 * reversible up until the tournament is marked in play.
 */
function pool_tournament_registration_self_form($form, &$form_state, $node) {
  global $user;
  $player = pool_tournament_player_for_user($user->uid);
  $form_state['node']   = $node;
  $form_state['player'] = $player;

  $registered = $player ? pool_tournament_registration_load($node->nid, $player->id()) : FALSE;

  $form['description'] = array(
    '#type'   => 'item',
    '#markup' => $registered
      ? t('You are registered for %title.', array('%title' => $node->title))
      : t('You are not registered for %title.', array('%title' => $node->title)),
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type'  => 'submit',
    '#value' => $registered ? t('Withdraw') : t('Register'),
  );
  return $form;
}

function pool_tournament_registration_self_form_submit($form, &$form_state) {
  $node   = $form_state['node'];
  $player = $form_state['player'];

  $registered = pool_tournament_registration_load($node->nid, $player->id());
  if ($registered) {
    pool_tournament_unregister_player($node->nid, $player->id());
    backdrop_set_message(t('You have withdrawn from %title.', array('%title' => $node->title)));
  }
  else {
    pool_tournament_register_player($node->nid, $player->id());
    backdrop_set_message(t('You are registered for %title.', array('%title' => $node->title)));
  }
  $form_state['redirect'] = 'node/' . $node->nid;
}

// ---------------------------------------------------------------------------
// TD/admin roster management
// ---------------------------------------------------------------------------

/**
 * Roster page: current registrations plus an add-player form. Available
 * to the TD/admin regardless of in_play state.
 */
function pool_tournament_tournament_roster_page($node) {
  $build = array();
  $build['add_form'] = backdrop_get_form('pool_tournament_roster_add_form', $node);

  $registrations = pool_tournament_tournament_registrations($node->nid);
  $rows = array();
  foreach ($registrations as $player_id => $reg) {
    $player = pool_tournament_player_load($player_id);
    $rows[] = array(
      $player ? check_plain($player->label) : t('(deleted player)'),
      check_plain($reg->fargo_rating_snapshot),
      format_date($reg->registered, 'short'),
      l(t('Remove'), 'node/' . $node->nid . '/roster/' . $player_id . '/remove'),
    );
  }
  $build['table'] = array(
    '#markup' => theme('table', array(
      'header' => array(t('Player'), t('Fargo (at registration)'), t('Registered'), t('Operations')),
      'rows'   => $rows,
      'empty'  => t('No players registered yet.'),
    )),
  );
  return $build;
}

function pool_tournament_roster_add_form($form, &$form_state, $node) {
  $form_state['node'] = $node;
  $options = array('' => t('- Select -'));
  foreach (entity_load_multiple('player') as $player) {
    $options[$player->id()] = check_plain($player->label());
  }
  $form['player_id'] = array(
    '#type'     => 'select',
    '#title'    => t('Add player'),
    '#options'  => $options,
    '#required' => TRUE,
  );
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Add to roster'));
  return $form;
}

function pool_tournament_roster_add_form_submit($form, &$form_state) {
  $node      = $form_state['node'];
  $player_id = $form_state['values']['player_id'];
  pool_tournament_register_player($node->nid, $player_id);
  $player = pool_tournament_player_load($player_id);
  backdrop_set_message(t('%label added to the roster.', array('%label' => $player ? $player->label : $player_id)));
  $form_state['redirect'] = 'node/' . $node->nid . '/roster';
}

/**
 * Confirm form for removing a player from a tournament's roster.
 */
function pool_tournament_roster_remove_form($form, &$form_state, $node, $player_id) {
  $player = pool_tournament_player_load($player_id);
  $form_state['node']      = $node;
  $form_state['player_id'] = $player_id;
  return confirm_form($form,
    t('Remove %label from this tournament?', array('%label' => $player ? $player->label : $player_id)),
    'node/' . $node->nid . '/roster',
    t('This will delete their registration record for this tournament.'),
    t('Remove'), t('Cancel')
  );
}

function pool_tournament_roster_remove_form_submit($form, &$form_state) {
  $node      = $form_state['node'];
  $player_id = $form_state['player_id'];
  pool_tournament_unregister_player($node->nid, $player_id);
  backdrop_set_message(t('Removed from the roster.'));
  $form_state['redirect'] = 'node/' . $node->nid . '/roster';
}

// ---------------------------------------------------------------------------
// "Tournament in Play" toggle
// ---------------------------------------------------------------------------

/**
 * TD/admin confirm form toggling the "Tournament in Play" flag. Setting it
 * blocks self-service registration/withdrawal; unsetting it reopens them.
 * The TD/admin's own ability to modify the roster via
 * pool_tournament_tournament_roster_page() is never affected either way.
 */
function pool_tournament_tournament_in_play_form($form, &$form_state, $node) {
  $form_state['node'] = $node;
  $in_play = pool_tournament_tournament_in_play($node->nid);

  if ($in_play) {
    return confirm_form($form,
      t('Reopen registration for %title?', array('%title' => $node->title)),
      'node/' . $node->nid,
      t('Players will be able to self-register and withdraw again.'),
      t('Reopen registration'), t('Cancel')
    );
  }
  return confirm_form($form,
    t('Mark %title as in play?', array('%title' => $node->title)),
    'node/' . $node->nid,
    t('Players will no longer be able to self-register or withdraw -- only you (or an admin) will be able to modify the roster from here on.'),
    t('Mark in play'), t('Cancel')
  );
}

function pool_tournament_tournament_in_play_form_submit($form, &$form_state) {
  $node        = $form_state['node'];
  $was_in_play = pool_tournament_tournament_in_play($node->nid);
  db_merge('pool_tournament_tournament_extra')
    ->key(array('nid' => $node->nid))
    ->fields(array('in_play' => $was_in_play ? 0 : 1))
    ->execute();
  backdrop_set_message($was_in_play
    ? t('Registration reopened for %title.', array('%title' => $node->title))
    : t('%title marked as in play.', array('%title' => $node->title)));
  $form_state['redirect'] = 'node/' . $node->nid;
}

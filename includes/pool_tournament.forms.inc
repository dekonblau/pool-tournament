<?php
/**
 * @file
 * Entity CRUD forms and player preference form.
 */

// ---------------------------------------------------------------------------
// Player forms
// ---------------------------------------------------------------------------

/**
 * Player add/edit form.
 */
function pool_tournament_player_form($form, &$form_state, $player) {
  $form_state['player'] = $player;

  $form['label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Full name'),
    '#default_value' => $player->label,
    '#required'      => TRUE,
  );

  // Link to a Backdrop user account (optional).
  $form['uid'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Backdrop user account'),
    '#description'   => t('Enter a username to link this player to a Backdrop user account (optional).'),
    '#default_value' => $player->uid ? _pool_tournament_uid_to_name($player->uid) : '',
    '#autocomplete_path' => 'user/autocomplete',
  );

  $form['fargo'] = array(
    '#type'  => 'fieldset',
    '#title' => t('Fargo Rate'),
  );
  $form['fargo']['member_id'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Member ID'),
    '#default_value' => $player->member_id,
    '#description'   => t('Fargo member ID (readableId). Leave blank and use the lookup below to auto-populate.'),
  );
  $form['fargo']['membership_id'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Membership ID'),
    '#default_value' => $player->membership_id,
    '#description'   => t("Fargo membershipId -- tied to a specific sanctioning body/league, not the player globally. Can't be used to search Fargo Rate; record-keeping only."),
  );
  $form['fargo']['fargo_rating'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Fargo Rating'),
    '#default_value' => $player->fargo_rating,
  );
  $form['fargo']['robustness'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Robustness'),
    '#default_value' => $player->robustness,
  );
  $form['fargo']['location'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Location'),
    '#default_value' => $player->location,
  );
  $form['fargo']['fargo_lookup'] = array(
    '#type'        => 'submit',
    '#value'       => t('Look up on Fargo Rate'),
    '#submit'      => array('pool_tournament_player_fargo_lookup_submit'),
    '#limit_validation_errors' => array(array('label'), array('member_id')),
  );
  // A prior lookup/selection leaves Member ID filled in with the found
  // readableId. Since the lookup submit handler searches by Member ID
  // first and only falls back to Full name when it's empty, that filled-in
  // ID quietly takes over every subsequent "Look up" click -- searching
  // "kevin stanelle" a second time would actually search the specific ID
  // left over from selecting Sr., returning only Sr. again. This button
  // clears the found Fargo fields (and any pending candidate table) so a
  // fresh name search isn't shadowed by a stale ID.
  $form['fargo']['fargo_clear'] = array(
    '#type'        => 'submit',
    '#value'       => t('Clear / new search'),
    '#submit'      => array('pool_tournament_player_fargo_clear_submit'),
    '#limit_validation_errors' => array(),
  );

  // A query like a common last name can match more than one person (e.g.
  // "Kevin Stanelle" matching both a Sr. and a Jr.) -- when that happens,
  // pool_tournament_player_fargo_lookup_submit() stashes every match here
  // instead of guessing, and this table lets the admin pick the right one.
  if (!empty($form_state['fargo_candidates'])) {
    $options = array();
    foreach ($form_state['fargo_candidates'] as $i => $candidate) {
      $options[$i] = array(
        'name'          => trim($candidate['firstName'] . ' ' . $candidate['lastName']),
        'location'      => isset($candidate['location']) ? $candidate['location'] : '',
        'rating'        => isset($candidate['effectiveRating']) ? $candidate['effectiveRating'] : '',
        'robustness'    => isset($candidate['robustness']) ? $candidate['robustness'] : '',
        'member_id'     => isset($candidate['readableId']) ? $candidate['readableId'] : '',
        'membership_id' => isset($candidate['membershipId']) ? $candidate['membershipId'] : '',
      );
    }
    $form['fargo']['fargo_candidates'] = array(
      '#type'     => 'tableselect',
      '#header'   => array(
        'name'          => t('Name'),
        'location'      => t('Location'),
        'rating'        => t('Rating'),
        'robustness'    => t('Robustness'),
        'member_id'     => t('Member ID'),
        'membership_id' => t('Membership ID'),
      ),
      '#options'  => $options,
      '#multiple' => FALSE,
      '#empty'    => t('No matches.'),
    );
    $form['fargo']['fargo_select'] = array(
      '#type'        => 'submit',
      '#value'       => t('Use selected match'),
      '#submit'      => array('pool_tournament_player_fargo_select_submit'),
      '#limit_validation_errors' => array(array('fargo_candidates')),
    );
  }

  // Attach field widgets (Field API fields).
  field_attach_form('player', $player, $form, $form_state);

  $form['status'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Active'),
    '#default_value' => $player->status,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save player'));
  if ($player->id()) {
    $form['actions']['delete'] = array(
      '#type'   => 'link',
      '#title'  => t('Delete'),
      '#href'   => 'player/' . $player->id() . '/delete',
      '#options' => array('attributes' => array('class' => array('button', 'button-danger'))),
    );
  }

  return $form;
}

/**
 * Fargo lookup button submit — populates fields from fargorate.com, or
 * presents a selectable list when the query matches more than one person
 * (e.g. a father and son sharing a name).
 */
function pool_tournament_player_fargo_lookup_submit($form, &$form_state) {
  module_load_include('inc', 'pool_tournament', 'includes/pool_tournament.fargo');

  $query = trim($form_state['values']['member_id']);
  if (empty($query)) {
    $query = trim($form_state['values']['label']);
  }

  if (empty($query)) {
    form_set_error('label', t('Enter a player name or Fargo member ID to look up.'));
    return;
  }

  $results = pool_tournament_fargo_lookup_all($query);
  unset($form_state['fargo_candidates']);

  if ($results === FALSE) {
    // Distinct from "no match" -- the service itself didn't respond.
    backdrop_set_message(t('Could not reach the Fargo Rate service. Try again shortly.'), 'error');
  }
  elseif (empty($results)) {
    backdrop_set_message(t('No Fargo profile found for %q.', array('%q' => $query)), 'warning');
  }
  elseif (count($results) === 1) {
    _pool_tournament_fargo_apply_candidate($results[0], $form_state);
  }
  else {
    // More than one match -- don't guess, let the admin pick.
    $form_state['fargo_candidates'] = $results;
    backdrop_set_message(t('@count Fargo profiles found for %q. Select the correct one below.',
      array('@count' => count($results), '%q' => $query)));
  }

  $form_state['rebuild'] = TRUE;
}

/**
 * "Use selected match" button submit — applies whichever row was picked
 * from the candidate table built in pool_tournament_player_form().
 */
function pool_tournament_player_fargo_select_submit($form, &$form_state) {
  // #multiple is FALSE on the tableselect, so it renders as radios and
  // submits the single selected row key as a plain scalar -- not the
  // checkbox-style {key: key-or-0} array a #multiple tableselect would
  // submit. Checked with !== '' rather than empty()/isset() alone since
  // index 0 (a legitimate row) is falsy.
  $index = isset($form_state['values']['fargo_candidates']) && $form_state['values']['fargo_candidates'] !== ''
    ? $form_state['values']['fargo_candidates']
    : NULL;

  if ($index === NULL || !isset($form_state['fargo_candidates'][$index])) {
    backdrop_set_message(t('Select one of the matches before continuing.'), 'warning');
  }
  else {
    _pool_tournament_fargo_apply_candidate($form_state['fargo_candidates'][$index], $form_state);
  }

  unset($form_state['fargo_candidates']);
  $form_state['rebuild'] = TRUE;
}

/**
 * "Clear / new search" button submit -- resets the found Fargo fields and
 * any pending candidate table so the next "Look up" click does a fresh
 * name search instead of being shadowed by a stale Member ID.
 */
function pool_tournament_player_fargo_clear_submit($form, &$form_state) {
  foreach (array('member_id', 'membership_id', 'fargo_rating', 'robustness', 'location') as $field) {
    $form_state['values'][$field] = '';
    // Both need clearing, not just values -- see the note in
    // _pool_tournament_fargo_apply_candidate() on why a rebuilt widget's
    // displayed value comes from $form_state['input'], not #default_value
    // or $form_state['values'].
    $form_state['input'][$field] = '';
  }
  unset($form_state['fargo_candidates']);
  backdrop_set_message(t('Cleared. Enter a new name or member ID and look up again.'));
  $form_state['rebuild'] = TRUE;
}

/**
 * Copies a single Fargo API record onto the form values and confirms it.
 *
 * Shared by the lookup submit (single-match case) and the candidate-table
 * select submit (multi-match case) so both paths populate identically.
 */
function _pool_tournament_fargo_apply_candidate(array $fargo, array &$form_state) {
  // Each button click is its own HTTP request/PHP process, so fargo.inc
  // being loaded during an earlier "Look up" submission doesn't carry over
  // to a later "Use selected match" submission -- load it here rather than
  // relying on every caller to remember to.
  module_load_include('inc', 'pool_tournament', 'includes/pool_tournament.fargo');
  $mapped = pool_tournament_fargo_map_to_player($fargo);
  foreach ($mapped as $field => $value) {
    $form_state['values'][$field] = $value;
    // Textfields (and most widgets) resolve their displayed value from
    // $form_state['input'] on any rebuild, not from #default_value or
    // $form_state['values'] -- see form_type_textfield_value() in core.
    // Without also setting this, the field keeps showing whatever the user
    // last typed (or nothing) instead of the looked-up Fargo data.
    $form_state['input'][$field] = $value;
  }
  backdrop_set_message(t('Fargo profile found: @name, @loc, rating @r, robustness @rb, membership @mid.',
    array(
      '@name' => trim($fargo['firstName'] . ' ' . $fargo['lastName']),
      '@loc'  => $mapped['location'],
      '@r'    => $mapped['fargo_rating'],
      '@rb'   => $mapped['robustness'],
      '@mid'  => $mapped['membership_id'],
    )
  ));
}

/**
 * Player form validation.
 */
function pool_tournament_player_form_validate($form, &$form_state) {
  // Resolve Backdrop username to uid.
  $username = trim($form_state['values']['uid']);
  if (!empty($username)) {
    $account = user_load_by_name($username);
    if (!$account) {
      form_set_error('uid', t('User %name not found.', array('%name' => $username)));
    }
    else {
      $form_state['values']['uid'] = $account->uid;
    }
  }
  else {
    $form_state['values']['uid'] = NULL;
  }

  field_attach_form_validate('player', $form_state['player'], $form, $form_state);
}

/**
 * Player form submit.
 */
function pool_tournament_player_form_submit($form, &$form_state) {
  $player = $form_state['player'];

  $player->label      = $form_state['values']['label'];
  $player->uid        = $form_state['values']['uid'];
  $player->member_id  = $form_state['values']['member_id'];
  $player->membership_id = $form_state['values']['membership_id'];
  $player->fargo_rating = $form_state['values']['fargo_rating'];
  $player->robustness = $form_state['values']['robustness'];
  $player->location   = $form_state['values']['location'];
  $player->status     = $form_state['values']['status'];

  field_attach_submit('player', $player, $form, $form_state);
  $player->save();

  backdrop_set_message(t('Player %name saved.', array('%name' => $player->label)));
  $form_state['redirect'] = 'player/' . $player->id();
}

/**
 * Player delete confirm form.
 */
function pool_tournament_player_delete_form($form, &$form_state, $player) {
  $form_state['player'] = $player;
  return confirm_form($form,
    t('Delete player %name?', array('%name' => $player->label)),
    'player/' . $player->id(),
    t('All field data for this player will be permanently deleted.'),
    t('Delete'), t('Cancel')
  );
}

function pool_tournament_player_delete_form_submit($form, &$form_state) {
  $player = $form_state['player'];
  $player->delete();
  backdrop_set_message(t('Player %name deleted.', array('%name' => $player->label)));
  $form_state['redirect'] = 'players';
}

// ---------------------------------------------------------------------------
// Player preferences form
// ---------------------------------------------------------------------------

/**
 * Form for a player to select their tournament preferences.
 *
 * @param object $player Player entity.
 */
function pool_tournament_player_preferences_form($form, &$form_state, $player) {
  $form_state['player'] = $player;

  $current = _pool_tournament_player_preferences_load($player->id());

  // Game type preferences.
  $game_types = _pool_tournament_entity_options('game_type');
  $form['game_types'] = array(
    '#type'          => 'checkboxes',
    '#title'         => t('Preferred game types'),
    '#options'       => $game_types,
    '#default_value' => isset($current['game_type']) ? $current['game_type'] : array(),
  );

  // Player type preferences.
  $player_types = _pool_tournament_entity_options('player_type');
  $form['player_types'] = array(
    '#type'          => 'checkboxes',
    '#title'         => t('Preferred player types'),
    '#options'       => $player_types,
    '#default_value' => isset($current['player_type']) ? $current['player_type'] : array(),
  );

  // Tournament type preferences.
  $tournament_types = _pool_tournament_entity_options('tournament_type');
  $form['tournament_types'] = array(
    '#type'          => 'checkboxes',
    '#title'         => t('Preferred tournament types'),
    '#options'       => $tournament_types,
    '#default_value' => isset($current['tournament_type']) ? $current['tournament_type'] : array(),
  );

  // Venue preferences.
  $venues = _pool_tournament_entity_options('venue');
  $form['venues'] = array(
    '#type'          => 'checkboxes',
    '#title'         => t('Preferred venues'),
    '#options'       => $venues,
    '#default_value' => isset($current['venue']) ? $current['venue'] : array(),
  );

  // Sanctioning body preferences.
  $sanctioning_bodies = _pool_tournament_entity_options('sanctioning_body');
  $form['sanctioning_bodies'] = array(
    '#type'          => 'checkboxes',
    '#title'         => t('Preferred sanctioning bodies'),
    '#options'       => $sanctioning_bodies,
    '#default_value' => isset($current['sanctioning_body']) ? $current['sanctioning_body'] : array(),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save preferences'));

  return $form;
}

function pool_tournament_player_preferences_form_submit($form, &$form_state) {
  $player    = $form_state['player'];
  $player_id = $player->id();

  // Delete existing preferences for this player.
  db_delete('pool_tournament_player_preference')
    ->condition('player_id', $player_id)
    ->execute();

  $map = array(
    'game_type'        => $form_state['values']['game_types'],
    'player_type'      => $form_state['values']['player_types'],
    'tournament_type'  => $form_state['values']['tournament_types'],
    'venue'            => $form_state['values']['venues'],
    'sanctioning_body' => $form_state['values']['sanctioning_bodies'],
  );

  $insert = db_insert('pool_tournament_player_preference')
    ->fields(array('player_id', 'preference_type', 'entity_id'));

  $has_rows = FALSE;
  foreach ($map as $ptype => $selected) {
    foreach (array_filter($selected) as $entity_id) {
      $insert->values(array($player_id, $ptype, (int) $entity_id));
      $has_rows = TRUE;
    }
  }
  if ($has_rows) {
    $insert->execute();
  }

  backdrop_set_message(t('Preferences saved for %name.', array('%name' => $player->label)));
}

// ---------------------------------------------------------------------------
// Generic entity forms for player_type, game_type, tournament_type, venue
// ---------------------------------------------------------------------------

/**
 * Generic add/edit form for simple reference entities.
 *
 * @param string $entity_type  E.g. 'game_type'.
 * @param object $entity       Entity object (new or loaded).
 * @param string $id_key       Primary key property name, e.g. 'game_type_id'.
 * @param string $cancel_path  Path to redirect on cancel.
 */
function pool_tournament_generic_entity_form($form, &$form_state, $entity_type, $entity, $id_key, $cancel_path) {
  $form_state['entity_type'] = $entity_type;
  $form_state['entity']      = $entity;
  $form_state['id_key']      = $id_key;

  $form['label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Label'),
    '#default_value' => $entity->label,
    '#required'      => TRUE,
  );

  if (isset($entity->description)) {
    $form['description'] = array(
      '#type'          => 'textarea',
      '#title'         => t('Description'),
      '#default_value' => $entity->description,
      '#rows'          => 3,
    );
  }

  if ($entity_type === 'player_type' && isset($entity->players_per_side)) {
    $fixed = _pool_tournament_player_type_fixed_players_per_side($entity->bundle);
    if ($fixed !== NULL) {
      // Singles and the partners family: the count is implied by the
      // bundle itself, so there's nothing for the admin to decide here.
      $form['players_per_side_display'] = array(
        '#type'   => 'item',
        '#title'  => t('Players per side'),
        '#markup' => $fixed,
      );
      $form['players_per_side'] = array(
        '#type'  => 'value',
        '#value' => $fixed,
      );
    }
    else {
      // Team: size is no longer a property of the player_type instance --
      // different tournaments using the Team format can want different
      // sizes, so it's entered per-tournament instead (see the tournament
      // node's "Team size" field / pool_tournament_tournament_extra).
      $form['players_per_side_display'] = array(
        '#type'   => 'item',
        '#title'  => t('Players per side'),
        '#markup' => t("Determined per tournament (see the tournament's Team size field)."),
      );
      $form['players_per_side'] = array(
        '#type'  => 'value',
        '#value' => $entity->players_per_side ?: 1,
      );
    }
  }

  if ($entity_type === 'venue') {
    $form['address'] = array(
      '#type'          => 'textfield',
      '#title'         => t('Address'),
      '#default_value' => isset($entity->address) ? $entity->address : '',
    );
    $form['city'] = array(
      '#type'          => 'textfield',
      '#title'         => t('City'),
      '#default_value' => isset($entity->city) ? $entity->city : '',
    );
    $form['state'] = array(
      '#type'          => 'textfield',
      '#title'         => t('State'),
      '#default_value' => isset($entity->state) ? $entity->state : '',
    );
    $form['phone'] = array(
      '#type'          => 'textfield',
      '#title'         => t('Phone'),
      '#default_value' => isset($entity->phone) ? $entity->phone : '',
    );
  }

  if ($entity_type === 'sanctioning_body') {
    $form['rulebook_url'] = array(
      '#type'          => 'url',
      '#title'         => t('Rulebook URL'),
      '#default_value' => isset($entity->rulebook_url) ? $entity->rulebook_url : '',
      '#description'   => t('Link to the official rules/rulebook for this sanctioning body.'),
    );
  }

  // Fargo rating ceiling only means something for the two Fargo-based
  // bundles -- not open, and not the plain (non-Fargo) handicapped bundle.
  // Lives in pool_tournament_tournament_type_fargo_limit, a class-table-
  // inheritance-style extension table, not a base column every bundle
  // carries. Persisted in pool_tournament_generic_entity_form_submit().
  if ($entity_type === 'tournament_type' && in_array($entity->bundle, array('fargo_limit', 'fargo_limit_handicapped'))) {
    $ceiling = NULL;
    if ($entity->id()) {
      $ceiling = db_select('pool_tournament_tournament_type_fargo_limit', 'f')
        ->fields('f', array('fargo_rating_ceiling'))
        ->condition('tournament_type_id', $entity->id())
        ->execute()
        ->fetchField();
    }
    $form['fargo_rating_ceiling'] = array(
      '#type'          => 'number',
      '#title'         => t('Fargo rating ceiling'),
      '#description'   => t('Maximum eligible Fargo effectiveRating for this tournament type.'),
      '#default_value' => ($ceiling !== FALSE && $ceiling !== NULL) ? $ceiling : '',
      '#min'           => 0,
      '#required'      => TRUE,
    );
  }

  field_attach_form($entity_type, $entity, $form, $form_state);

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save'));
  $form['actions']['cancel'] = array(
    '#type'  => 'link',
    '#title' => t('Cancel'),
    '#href'  => $cancel_path,
  );

  return $form;
}

function pool_tournament_generic_entity_form_validate($form, &$form_state) {
  field_attach_form_validate($form_state['entity_type'], $form_state['entity'], $form, $form_state);
}

function pool_tournament_generic_entity_form_submit($form, &$form_state) {
  $entity_type = $form_state['entity_type'];
  $entity      = $form_state['entity'];
  $id_key      = $form_state['id_key'];

  $entity->label = $form_state['values']['label'];

  if (isset($entity->description)) {
    $entity->description = isset($form_state['values']['description']) ? $form_state['values']['description'] : '';
  }
  if ($entity_type === 'player_type') {
    $entity->players_per_side = $form_state['values']['players_per_side'];
  }
  if ($entity_type === 'venue') {
    foreach (array('address', 'city', 'state', 'phone') as $f) {
      if (isset($form_state['values'][$f])) {
        $entity->$f = $form_state['values'][$f];
      }
    }
  }
  if ($entity_type === 'sanctioning_body' && isset($form_state['values']['rulebook_url'])) {
    $entity->rulebook_url = $form_state['values']['rulebook_url'];
  }

  field_attach_submit($entity_type, $entity, $form, $form_state);
  $entity->save();

  // Needs the entity's own ID, which only exists after save() -- for a
  // brand-new entity, id() is NULL until this point.
  if ($entity_type === 'tournament_type' && isset($form_state['values']['fargo_rating_ceiling'])) {
    db_merge('pool_tournament_tournament_type_fargo_limit')
      ->key(array('tournament_type_id' => $entity->id()))
      ->fields(array('fargo_rating_ceiling' => $form_state['values']['fargo_rating_ceiling']))
      ->execute();
  }

  backdrop_set_message(t('%type %label saved.', array('%type' => $entity_type, '%label' => $entity->label)));

  $path_map = array(
    'player_type'     => 'player-type',
    'game_type'       => 'game-type',
    'tournament_type' => 'tournament-type',
    'venue'           => 'venue',
    'sanctioning_body' => 'sanctioning-body',
  );
  $base = isset($path_map[$entity_type]) ? $path_map[$entity_type] : $entity_type;
  $form_state['redirect'] = $base . '/' . $entity->id();
}

// ---------------------------------------------------------------------------
// Tournament node form alterations
// ---------------------------------------------------------------------------

/**
 * Alters the tournament node form to use entity select widgets.
 */
function pool_tournament_tournament_node_form_alter(&$form, &$form_state) {
  // Replace raw integer fields with select lists for entity references.
  foreach (array(
    'field_game_type_id'       => array('entity_type' => 'game_type',       'title' => t('Game Type')),
    'field_player_type_id'     => array('entity_type' => 'player_type',     'title' => t('Player Type')),
    'field_tournament_type_id' => array('entity_type' => 'tournament_type', 'title' => t('Tournament Type')),
    'field_venue_id'           => array('entity_type' => 'venue',           'title' => t('Venue')),
    'field_sanctioning_body_id' => array('entity_type' => 'sanctioning_body', 'title' => t('Sanctioning Body')),
  ) as $field_name => $info) {
    if (isset($form[$field_name])) {
      $options   = array('' => t('- Select -')) + _pool_tournament_entity_options($info['entity_type']);
      $current   = isset($form[$field_name][LANGUAGE_NONE][0]['value']['#default_value'])
        ? $form[$field_name][LANGUAGE_NONE][0]['value']['#default_value']
        : '';
      $form[$field_name][LANGUAGE_NONE][0]['value'] = array(
        '#type'          => 'select',
        '#title'         => $info['title'],
        '#options'       => $options,
        '#default_value' => $current,
      );
    }
  }

  // Team size lives in pool_tournament_tournament_extra, not a Field API
  // field (see _pool_tournament_save_tournament_extra()) -- only meaningful
  // when the selected player_type resolves to bundle 'team', enforced in
  // pool_tournament_tournament_team_size_validate() below rather than with
  // client-side #states, since the player_type select's options are entity
  // IDs, not bundle names, so there's no static value to key #states off of.
  $default_team_size = NULL;
  if (!empty($form['#node']->nid)) {
    $default_team_size = db_select('pool_tournament_tournament_extra', 'e')
      ->fields('e', array('team_size'))
      ->condition('nid', $form['#node']->nid)
      ->execute()
      ->fetchField();
  }
  $form['team_size'] = array(
    '#type'          => 'number',
    '#title'         => t('Team size'),
    '#description'   => t('Only used when Player Type is Team. Must be greater than 2.'),
    '#default_value' => $default_team_size ?: '',
    '#weight'        => isset($form['field_player_type_id']['#weight']) ? $form['field_player_type_id']['#weight'] + 1 : 0,
  );
  $form['#validate'][] = 'pool_tournament_tournament_team_size_validate';
}

/**
 * Validate handler for the tournament node form's team_size field.
 *
 * Requires a value > 2 when the selected player_type's bundle is 'team';
 * clears any submitted value otherwise so a stale number left over from a
 * previous edit doesn't get persisted for a non-team tournament.
 */
function pool_tournament_tournament_team_size_validate($form, &$form_state) {
  $player_type_id = isset($form_state['values']['field_player_type_id'][LANGUAGE_NONE][0]['value'])
    ? $form_state['values']['field_player_type_id'][LANGUAGE_NONE][0]['value']
    : NULL;

  $is_team = FALSE;
  if (!empty($player_type_id)) {
    $player_type = entity_load('player_type', $player_type_id);
    if ($player_type && $player_type->bundle() === 'team') {
      $is_team = TRUE;
    }
  }

  if ($is_team) {
    $size = isset($form_state['values']['team_size']) ? (int) $form_state['values']['team_size'] : 0;
    if ($size <= 2) {
      form_set_error('team_size', t('Team size must be greater than 2 for a Team-format tournament.'));
    }
  }
  else {
    $form_state['values']['team_size'] = NULL;
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Returns the fixed players-per-side count implied by a player_type bundle,
 * or NULL if the bundle's team size varies and must be entered by the admin.
 */
function _pool_tournament_player_type_fixed_players_per_side($bundle) {
  $fixed = array(
    'singles'               => 1,
    'partners'               => 2,
    'partners_general'       => 2,
    'partners_mixed'         => 2,
    'partners_scotch'        => 2,
    'partners_mixed_scotch'  => 2,
  );
  return isset($fixed[$bundle]) ? $fixed[$bundle] : NULL;
}

/**
 * Returns an options array (id => label) for a given entity type.
 */
function _pool_tournament_entity_options($entity_type) {
  $entities = entity_load_multiple($entity_type);
  $options  = array();
  foreach ($entities as $entity) {
    $options[$entity->id()] = check_plain($entity->label());
  }
  return $options;
}

/**
 * Returns a player's Backdrop preferences from the preference table.
 *
 * @return array  Keyed by preference_type => array of entity_ids.
 */
function _pool_tournament_player_preferences_load($player_id) {
  $result = db_select('pool_tournament_player_preference', 'p')
    ->fields('p', array('preference_type', 'entity_id'))
    ->condition('p.player_id', $player_id)
    ->execute();

  $prefs = array();
  foreach ($result as $row) {
    $prefs[$row->preference_type][] = $row->entity_id;
  }
  return $prefs;
}

/**
 * Resolves a Backdrop uid to a username string.
 */
function _pool_tournament_uid_to_name($uid) {
  $account = user_load($uid);
  return $account ? $account->name : '';
}

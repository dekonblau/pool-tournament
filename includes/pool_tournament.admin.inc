<?php
/**
 * @file
 * Admin forms: module settings and bundle management.
 */

// ---------------------------------------------------------------------------
// Module settings
// ---------------------------------------------------------------------------

/**
 * Settings form for bracket manager API connection and Fargo.
 */
function pool_tournament_settings_form($form, &$form_state) {
  $config = config('pool_tournament.settings');

  $form['bracket'] = array(
    '#type'  => 'fieldset',
    '#title' => t('Bracket Manager API'),
  );
  $form['bracket']['bracket_api_base'] = array(
    '#type'          => 'textfield',
    '#title'         => t('API Base URL'),
    '#default_value' => $config->get('bracket_api_base') ?: 'http://localhost:3001/api',
    '#description'   => t('Base URL of the bracket manager API. Use http://localhost:3001/api when both apps run on the same server.'),
    '#required'      => TRUE,
  );
  $form['bracket']['bracket_api_key'] = array(
    '#type'          => 'textfield',
    '#title'         => t('API Key (X-Api-Key)'),
    '#default_value' => $config->get('bracket_api_key'),
    '#description'   => t('Shared secret sent as X-Api-Key header on all mutating requests.'),
    '#required'      => TRUE,
  );
  $form['bracket']['bracket_ping'] = array(
    '#type'   => 'submit',
    '#value'  => t('Test connection'),
    '#submit' => array('pool_tournament_settings_ping_submit'),
    '#limit_validation_errors' => array(),
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save configuration'));

  return $form;
}

function pool_tournament_settings_form_submit($form, &$form_state) {
  config('pool_tournament.settings')
    ->set('bracket_api_base', $form_state['values']['bracket_api_base'])
    ->set('bracket_api_key',  $form_state['values']['bracket_api_key'])
    ->save();
  backdrop_set_message(t('Settings saved.'));
}

function pool_tournament_settings_ping_submit($form, &$form_state) {
  module_load_include('inc', 'pool_tournament', 'includes/pool_tournament.bracket');
  if (pool_tournament_bracket_ping()) {
    backdrop_set_message(t('Bracket manager API is reachable.'));
  }
  else {
    backdrop_set_message(t('Could not reach the bracket manager API. Check the base URL and ensure the server is running.'), 'error');
  }
}

// ---------------------------------------------------------------------------
// Generic bundle management (shared across entity types)
// ---------------------------------------------------------------------------

/**
 * Lists bundles for an entity type.
 *
 * @param string $entity_type  E.g. 'player_type'.
 * @param string $config_prefix Config prefix, e.g. 'player_type.bundle'.
 * @param string $add_path     Path to the add-bundle form.
 */
function pool_tournament_bundle_list_page($entity_type, $config_prefix, $add_path) {
  $bundles = _pool_tournament_bundle_load_all($config_prefix);
  $rows    = array();
  foreach ($bundles as $bundle => $info) {
    $rows[] = array(
      check_plain($info['label']),
      check_plain($bundle),
      l(t('Edit'), $add_path . '/' . $bundle . '/edit') . ' | ' .
      l(t('Delete'), $add_path . '/' . $bundle . '/delete'),
    );
  }
  $build['table'] = array(
    '#theme'  => 'table',
    '#header' => array(t('Label'), t('Machine name'), t('Operations')),
    '#rows'   => $rows,
    '#empty'  => t('No bundles defined yet.'),
  );
  $build['add_link'] = array(
    '#markup' => l(t('Add bundle'), $add_path . '/add', array('attributes' => array('class' => array('button')))),
  );
  return $build;
}

/**
 * Form to add or edit a bundle.
 */
function pool_tournament_bundle_form($form, &$form_state, $config_prefix, $bundle_id = NULL) {
  $form_state['config_prefix'] = $config_prefix;
  $form_state['bundle_id']     = $bundle_id;

  $existing = $bundle_id ? config_get($config_prefix . '.' . $bundle_id) : array();

  $form['label'] = array(
    '#type'          => 'textfield',
    '#title'         => t('Label'),
    '#default_value' => isset($existing['label']) ? $existing['label'] : '',
    '#required'      => TRUE,
  );
  $form['bundle'] = array(
    '#type'          => 'machine_name',
    '#title'         => t('Machine name'),
    '#default_value' => $bundle_id ?: '',
    '#disabled'      => (bool) $bundle_id,
    '#machine_name'  => array(
      'exists'    => '_pool_tournament_bundle_exists',
      'source'    => array('label'),
      'replace'   => '_',
      'lowercase' => TRUE,
    ),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save'));
  $form['actions']['cancel'] = array(
    '#type'  => 'link',
    '#title' => t('Cancel'),
    '#href'  => isset($_GET['destination']) ? $_GET['destination'] : '<front>',
  );

  return $form;
}

function pool_tournament_bundle_form_submit($form, &$form_state) {
  $config_prefix = $form_state['config_prefix'];
  $bundle        = $form_state['values']['bundle'];
  $label         = $form_state['values']['label'];
  $is_new        = empty($form_state['bundle_id']);

  $config_name = $config_prefix . '.' . $bundle;
  config($config_name)
    ->set('_config_name', $config_name)
    ->set('bundle', $bundle)
    ->set('label', $label)
    ->save();

  if ($is_new) {
    // Determine entity type from config prefix (e.g. 'player_type.bundle' → 'player_type').
    $entity_type = strstr($config_prefix, '.', TRUE);
    field_attach_create_bundle($entity_type, $bundle);
    backdrop_set_message(t('Bundle %label created.', array('%label' => $label)));
  }
  else {
    backdrop_set_message(t('Bundle %label updated.', array('%label' => $label)));
  }
  backdrop_static_reset();
}

/**
 * Confirm form to delete a bundle.
 */
function pool_tournament_bundle_delete_form($form, &$form_state, $config_prefix, $bundle_id) {
  $form_state['config_prefix'] = $config_prefix;
  $form_state['bundle_id']     = $bundle_id;
  $info = config_get($config_prefix . '.' . $bundle_id);
  return confirm_form($form,
    t('Delete bundle %label?', array('%label' => $info['label'])),
    isset($_GET['destination']) ? $_GET['destination'] : '<front>',
    t('This will remove the bundle and all field instances attached to it. This action cannot be undone.'),
    t('Delete'), t('Cancel')
  );
}

function pool_tournament_bundle_delete_form_submit($form, &$form_state) {
  $config_prefix = $form_state['config_prefix'];
  $bundle_id     = $form_state['bundle_id'];
  $entity_type   = strstr($config_prefix, '.', TRUE);

  config($config_prefix . '.' . $bundle_id)->delete();
  field_attach_delete_bundle($entity_type, $bundle_id);
  backdrop_set_message(t('Bundle %id deleted.', array('%id' => $bundle_id)));
  backdrop_static_reset();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

function _pool_tournament_bundle_load_all($config_prefix) {
  $bundles = array();
  $names   = config_get_names_with_prefix($config_prefix . '.');
  foreach ($names as $name) {
    $config = config_get($name);
    if (!empty($config['bundle'])) {
      $bundles[$config['bundle']] = $config;
    }
  }
  return $bundles;
}

function _pool_tournament_bundle_exists($value, $element, $form_state) {
  $prefix = $form_state['config_prefix'];
  $config = config_get($prefix . '.' . $value);
  return !empty($config);
}

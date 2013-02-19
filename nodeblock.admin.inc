<?php

/**
 * @file
 * Da admin forms.
 */

/**
 * Global admin forms.
 */
function nodeblock_admin_form($form, &$form_state) {

  $cache_options = array(
    DRUPAL_NO_CACHE                               => t("No cache"),
    DRUPAL_CACHE_GLOBAL                           => t("Global (bypasses access)"),
    DRUPAL_CACHE_PER_PAGE                         => t("Per page (bypasses access)"),
    DRUPAL_CACHE_PER_ROLE                         => t("Per role"),
    DRUPAL_CACHE_PER_USER                         => t("Per user"),
    DRUPAL_CACHE_PER_ROLE | DRUPAL_CACHE_PER_PAGE => t("Per role and page"),
    DRUPAL_CACHE_PER_USER | DRUPAL_CACHE_PER_PAGE => t("Per user and page"),
  );

  $form[NODEBLOCK_CACHE_VAR] = array(
    '#type'          => 'select',
    '#title'         => t("Cache mode"),
    '#options'       => $cache_options,
    '#default_value' => variable_get(NODEBLOCK_CACHE_VAR, NODEBLOCK_CACHE_DEFAULT),
    '#description'   => t("This cache mode will be applied to all node block per default, change this and all blocks will inherit from the new mode. If you are working on a translated site, choose no cache. For advanced cache tuning, use the <em>hook_block_info_alter()</em> hook instead."),
  );

  $form[NODEBLOCK_VAR_TYPES] = array(
    '#type'          => 'checkboxes',
    '#title'         => t("Enabled on node types"),
    '#options'       => node_type_get_names(),
    '#default_value' => variable_get(NODEBLOCK_VAR_TYPES, array()),
    '#description'   => t("If you select nothing, every node type can have an associated block. By selecting one or more items in this variable, you will restrict node types to only the selected ones."),
  );

  $form[NODEBLOCK_VAR_DISPLAY_LINKS] = array(
    '#type'          => 'checkbox',
    '#title'         => t("Display links on node page"),
    '#default_value' => variable_get(NODEBLOCK_VAR_DISPLAY_LINKS, NODEBLOCK_DEF_DISPLAY_LINKS),
    '#description'   => t("If unchecked, the node actions links on node page won't be displayed anymore."),
  );

  $form['#validate'][] = 'nodeblock_admin_form_validate';

  return system_settings_form($form);
}

/**
 * Validate handler for global admin form.
 */
function nodeblock_admin_form_validate($form, &$form_state) {
  $enabled_types = array();
  foreach ($form_state['values'][NODEBLOCK_VAR_TYPES] as $type => $enabled) {
    if ($type === $enabled) {
      $enabled_types[$type] = $type;
    }
  }
  $form_state['values'][NODEBLOCK_VAR_TYPES] = $enabled_types;
}

/**
 * Overview and audit table.
 */
function nodeblock_admin_reports_form($form, &$form_state) {

  $header = array(
    'nid' => array(
      'data'  => t("Node Id."),
      'field' => 'nb.nid',
      'sort'  => 'asc',
    ),
    'title' => array(
      'data'  => t("Node title"),
      'field' => 'n.title',
    ),
    'delta' => array(
      'data'  => t("Delta"),
      'field' => 'nb.delta',
    ),
    'info' => array(
      'data'  => t("Admin info"),
      'field' => 'nb.title',
    ),
    'status' => array(
      'data'  => t("Published"),
      'field' => 'n.status',
    ),
    'state' => array(
      'data'  => t("Status"),
    ),
    'op' => array(
      'data'  => t("Operations"),
    ),
  );

  $query = db_select('nodeblock', 'nb');
  $query->fields('nb');
  $query->addField('n', 'status');
  $query->addField('n', 'title', 'node_title');
  $query->leftJoin('node', 'n', "n.nid = nb.nid");

  $result = $query
    ->extend('PagerDefault')
    ->extend('TableSort')
    ->limit(50)
    ->orderByHeader($header)
    ->addTag('node_access')
    ->execute();

  $rows = array();

  while ($data = $result->fetchObject()) {

    $broken = (null === $data->node_title);

    $row = array(
      'nid'    => $data->nid,
      'title'  => l(check_plain($data->node_title), 'node/' . $data->nid),
      'delta'  => check_plain($data->delta),
      'info'   => check_plain($data->title),
      'status' => $data->status ? t("Yes") : t("No"),
      'state'  => $broken ? t("Broken") : t("Ok"),
    );

    $links = array();

    if (!$broken) {
      $links[] = array(
        'href'  => 'node/' . $data->nid . '/edit',
        'title' => t("Edit node"),
      );
    }
    $row['op'] = theme('links', array('links' => $links));

    $rows[$data->nid] = $row;
  }

  $form['instances'] = array(
    '#type'    => 'tableselect',
    '#header'  => $header,
    '#options' => $rows,
  );

  return $form;
}

<?php

/**
 * @file
 * User pages.
 */

/**
 * Form element validate helper that prunes empty values.
 */
function nodeblock_edit_element_validate_filter_empty($element, &$form_state, $form) {
  form_set_value($element, array_filter($element['#value'], function ($value) {
    return !empty($value);
  }), $form_state);
}

/**
 * Menu item get trail.
 *
 * @param array $item
 *   Fully loaded menu item.
 *
 * @return array
 *   Complete item trail including the given item as last item.
 */
function _nodeblock_edit_menu_item_get_trail($item) {
  $trail = array();

  do {
    // Sometime the 'module' entry is missing, and that makes the menu.inc
    // functions to be angry and throw sad PHP noticies. Drupal is fucked up
    // and rotten to the roots.
    if (!isset($item['module'])) {
      $item['module'] = 'none';
    }
    array_unshift($trail, $item);
  } while ($item = _menu_link_find_parent($item));

  array_unshift($trail, array(
    'title'             => t('Home'),
    'href'              => '<front>',
    'link_path'         => '',
    'localized_options' => array(),
    'type'              => 0,
  ));

  return $trail;
}

/**
 * Set breadcrumb and title for nodeblock_edit forms.
 *
 * @param string $message
 *   Not translated message with optionnaly the %page variable embedded.
 * @param array $variables
 *   t() variables for message, if there are more than just %page.
 * @param string $fallback
 *   Fallback not translated message if any. 
 */
function _nodeblock_edit_form_set_breadcrumb($message, array $variables = array(), $fallback = null) {
  if (isset($_GET['destination']) && ($item = menu_get_item($_GET['destination']))) {
    if (false !== strpos($message, '%page')) {
      $variables['%page'] = $item['title'];
    }
    drupal_set_title(t($message, $variables), PASS_THROUGH);

    $breadcrumb = array();
    foreach (_nodeblock_edit_menu_item_get_trail($item) as $_item) {
      $breadcrumb[] = l($_item['title'], $_item['href'], $_item['localized_options']);
    }
    drupal_set_breadcrumb($breadcrumb);
  } else if ($fallback) {
    drupal_set_message(t($fallback));
  }
}

/**
 * Settings form.
 */
function nodeblock_edit_page_settings_form($form, &$form_state) {

  $options = array();
  $themes  = list_themes();
  foreach (list_themes() as $theme_key => $theme) {
    // Ignore disabled themes.
    if (!$theme->status) {
      continue;
    }
    // Fetch all regions.
    foreach (system_region_list($theme_key) as $region => $description) {
      $options[$region . '.' . $theme_key] = $theme->info['name'] . ': ' . $description;
    }
  }

  $form['nodeblock_region_link'] = array(
    '#type' => 'checkbox',
    '#title' => t("Display add link in regions"),
    '#default_value' => variable_get(NODEBLOCK_EDIT_VAR_REGION_LINK, NODEBLOCK_DEF_VAR_REGION_LINK),
    '#description' => t("If checked, a link will be displayed into the enabled regions by hacking core templating system. This means that if the region does not carry any block, the link won't be displayed. <em>We recommend that you place the add link block instead.</em>"),
  );

  $form['nodeblock_edit_enabled_regions'] = array(
    '#type'             => 'checkboxes',
    '#title'            => t("Enabled regions"),
    '#options'          => $options,
    '#default_value'    => variable_get(NODEBLOCK_EDIT_VAR_REGION_ENABLED, array()),
    '#element_validate' => array('nodeblock_edit_element_validate_filter_empty'),
  );

  return system_settings_form($form);
}


/**
 * Single region form.
 */
function nodeblock_edit_page_region_form($form, &$form_state, $region_info) {
  return $form;
}

/**
 * Get all valid aliases for a certain path.
 *
 * @param string $path
 *   Existing Drupal path.
 *
 * @return array
 *   Non ordered list of path.
 */
function nodeblock_edit_path_get_all_aliases($path) {
  $ret = array();

  // First find path aliases.
  /*
  if (($alias = drupal_get_path_alias($path)) && $alias !== $path) {
    $ret[] = $alias;
  }
   */
  $ret[] = drupal_get_normal_path($path);

  // Then try with i18n module.
  if (drupal_multilingual() && module_exists('i18n')) {
    foreach (i18n_get_path_translations($path) as $alias) {

      $ret[] = $alias['href'];

      // Dear i18n, let me tell you how much I hate you.
      $language = NULL;
      if (isset($alias['object'])) {
        if (is_object($alias['object']) && isset($alias['object']->language)) {
          $language = $alias['object']->language;
        } else if (is_array($alias['object']) && isset($alias['object']['language'])) {
          $language = $alias['object']['language'];
        } else {
          // HO MY GOD! EASTER EGG!
          watchdog('easter_egg', 'SUPPLIES! WTF I18N!', NULL);
        }
      }
      if (NULL !== $language) {
        $ret[] = drupal_get_path_alias($alias['href'], $language);
      }
    }
  }

  return array_unique($ret);
}

/**
 * Add block into region for only one page.
 */
function nodeblock_edit_page_region_add_page_form($form, &$form_state, $region_info) {

  _nodeblock_edit_form_set_breadcrumb("Add a block on page %page", array(), "Add a block");

  $form['pages'] = array(
    '#title'       => t("Pages"),
    '#type'        => 'textarea',
    '#rows'        => 3,
    '#description' => t("All pages this block will be visible, write one path per line."),
  );

  // Path introspection if destination is given.
  // We explicitely disallow automatic introspection for admin in order
  // to avoid the field going disabled under certain possible circumstances.
  if (isset($_GET['destination']) && 0 !== strpos($_GET['destination'], 'admin')) {

    $path = drupal_get_normal_path($_GET['destination']);

    // Find all aliases, especially i18n aliases.
    $aliases = nodeblock_edit_path_get_all_aliases($path);
    if (!in_array($path, $aliases)) {
      array_unshift($aliases, $_GET['destination']);
    }

    $form['pages']['#default_value'] = implode("\n", $aliases);
    $form['pages']['#description']  .= '<br/><strong>' . t("Path aliases have been auto-detected from the page you come from.") . '</strong>';
    $form['pages']['#disabled']      = TRUE;
    $form['pages']['#rows']          = count($aliases);
  }

  $form['id'] = array(
    '#title'             => t("Search for an existing bloc"),
    '#type'              => 'textfield',
    '#autocomplete_path' => 'admin/structure/region/block/autocomplete',
    '#element_validate'  => array('nodeblock_edit_block_id_element_validate'),
    '#description'       => t("Search for pages on your site."),
  );

  $form['region_info'] = array(
    '#type'  => 'value',
    '#value' => $region_info,
  );

  $form['actions'] = array(
    '#type'  => 'actions',
    'submit' => array(
      '#type'   => 'submit',
      '#value'  => t("Add block"),
      '#submit' => array('nodeblock_edit_page_region_add_page_form_submit_add'),
    ),
  );

  return $form;
}

/**
 * Add block into region for only one page submit handler.
 */
function nodeblock_edit_page_region_add_page_form_submit_add($form, &$form_state) {

  list($module, $id)    = explode('.', $form_state['values']['id'], 2);
  list($region, $theme) = $form_state['values']['region_info'];
  $pages                = $form_state['values']['pages'];

  if (!$delta = nodeblock_edit_ensure_block($module, $id)) {
    drupal_set_message(t("Could not create the block"), 'error');
    return;
  }

  foreach (array_filter(explode("\n", $pages)) as $path) {

    $fields = array(
      'module'    => $module,
      'delta'     => $delta,
      'weight'    => 0,
      'region'    => $region,
      'path'      => $path,
      'path_hash' => nodeblock_edit_hash($path),
    );

    // Add or ensure the block to be into the {blocks} table, then enable it
    // and clear associated cache.
    db_merge('nodeblock_edit_path')
      ->key(array(
        'module' => $module,
        'delta'  => $delta,
        'region' => $region,
        'path'   => $path,
      ))
      ->fields($fields)
      ->execute();
  }

  cache_clear_all();
  drupal_set_message(t("The new block has been added on this page."));
}

/**
 * Edit or move block into region.
 */
function nodeblock_edit_page_region_block_form($form, &$form_state, $region_info) {

  if (!isset($_GET['destination'])) {
    return MENU_NOT_FOUND;
  }

  _nodeblock_edit_form_set_breadcrumb("Move blocks on page %page", array(), "Move blocks");

  $path = drupal_get_normal_path($_GET['destination']);

  list($region) = $region_info;

  $form['#tree']  = TRUE;
  $form['#theme'] = 'nodeblock_edit_page_region_block_form';

  $custom = db_select('nodeblock_edit_path', 'np')
    ->fields('np')
    ->condition('np.path_hash', nodeblock_edit_hash($path))
    ->orderBy('np.weight')
    ->execute()
    ->fetchAll();

  $modules = module_list();
  foreach ($custom as $info) {
    $id = $block_id = $info->module . '.' . $info->delta;

    $form['blocks'][$id]['weight'] = array(
      '#type'          => 'weight',
      '#title'         => t('Weight for @title', array('@title' => $info->info)),
      '#title_display' => 'invisible',
      '#default_value' => $info->weight,
    );
    $form['blocks'][$id]['info'] = array(
      '#markup'        => nodeblock_edit_block_find_title($info->module, $info->delta),
    );
    $form['blocks'][$id]['module'] = array(
      '#markup'        => $modules[$info->module],
    );
  }

  $form['region_info'] = array(
    '#type'  => 'value',
    '#value' => $region_info,
  );
  $form['path'] = array(
    '#type'  => 'value',
    '#value' => $path,
  );

  $form['actions'] = array(
    '#type'  => 'actions',
    'submit' => array(
      '#type'   => 'submit',
      '#value'  => t("Save changes"),
      '#submit' => array('nodeblock_edit_page_region_block_form_submit_edit'),
    ),
  );

  return $form;
}

/**
 * Edit or move block into region submit handler.
 */
function nodeblock_edit_page_region_block_form_submit_edit($form, &$form_state) {

  list($region, $theme) = $form_state['values']['region_info'];

  $path = $form_state['values']['path'];

  foreach ($form_state['values']['blocks'] as $id => $data) {
    if (isset($data['weight'])) {

      list($module, $delta) = explode('.', $id, 2);

      db_update('nodeblock_edit_path')
        ->fields(array(
          'weight' => (int)$data['weight'],
        ))
        ->condition('path',  $path)
        ->condition('module', $module)
        ->condition('delta',  $delta)
        ->condition('region', $region)
        ->execute();
    }
  }

  drupal_set_message(t("Blocks have been moved."));

  // Empty block cache.
  cache_clear_all();
}

/**
 * Render the form table drag.
 */
function theme_nodeblock_edit_page_region_block_form(&$variables) {
  $form = &$variables['form'];

  $rows = array();
  foreach (element_children($form['blocks']) as $id) {
    $form['blocks'][$id]['weight']['#attributes']['class'][] = 'block-edit-weight';
    $rows[] = array(
      'data' => array(
        drupal_render($form['blocks'][$id]['info']),
        drupal_render($form['blocks'][$id]['weight']),
        drupal_render($form['blocks'][$id]['module']),
      ),
      'class' => array('draggable'),
    );
  }

  drupal_add_tabledrag('block-edit-order', 'order', 'sibling', 'block-edit-weight');

  return theme('table', array(
    'header' => array(
      t("Info"),
      t("Weight"),
      t("Module"),
    ),
    'rows'       => $rows,
    'attributes' => array('id' => 'block-edit-order'))
  ) . drupal_render_children($form);
}

/**
 * Autocomplete callback for blocks.
 */
function nodeblock_edit_autocomplete($region_info, $string) {

  $matches  = array();
  $language = $GLOBALS['language']->language;
  $string   = strtolower($string);

  list($region, $theme) = $region_info;

  /*
   * FIXME: Sorry, but we don't need this in the end.
   *
  // First select into block info.
  // Ugly info fetching.
  $block_list = array();

  foreach (module_implements('block_info') as $module) {
    $module_blocks = module_invoke($module, 'block_info');
    foreach ($module_blocks as $delta => $block) {
      $block['module'] = $module;
      $block['delta']  = $delta;
      $block['theme']  = $theme;
      $block_list[$module . '.' . $delta] = $block;
    }
  }
  drupal_alter('block_info', $block_list, $theme);

  // Prune blocks that don't match our substring.
  $block_list = array_filter($block_list, function ($block) use ($string) {
    return false !== strpos(strtolower($block['info']), $string);
  });

  // Get it all, poluting memory wins.
  foreach ($block_list as $key => $block) {
    $matches[$key] = $block['info'];
  }
   */

  // Proceed with alternative providers.
  // First (and only) is nodeblock.
  if (module_exists('nodeblock')) {

    // WARNING: UGLY
    $select = db_select('node', 'n');
    $select
      ->leftJoin('nodeblock', 'b', 'b.nid = n.nid');
    $select
      ->fields('n', array('nid', 'title', 'language', 'tnid'))
      ->fields('b', array('delta'));
    if ($types = variable_get(NODEBLOCK_VAR_TYPES, array())) {
      $select
        ->condition('n.type', array_values($types));
    }
    $select
      ->where('LOWER(n.title) LIKE :string', array(
        ':string' => '%' . db_like($string) . '%',
      ));
    if (drupal_multilingual() && module_exists('i18n')) {
      $select
        ->orderBy("(
              CASE n.language
                WHEN '" . db_escape_field($language) . "' THEN 0
                ELSE 1
              END
            )") // FIXME: HACK.
        ->groupBy('n.tnid');
    }
    $result = $select
      ->execute()
      ->fetchAll();

    /*
    // FIXME: Totally unacceptable query if there was millions of nodes.
    $query = "
      SELECT * FROM (
        SELECT n.nid, n.title, n.language, b.delta, n.tnid
          FROM {node} n
          LEFT JOIN {nodeblock} b ON b.nid = n.nid
          WHERE LOWER(n.title) LIKE :string
            AND n.type IN (:types)
          ORDER BY (
            CASE n.language
              WHEN :lang THEN 0
              ELSE 1
            END
          ) ASC
        ) AS a
        GROUP BY tnid
      ";

    $result = db_query($query, array(
      ':string' => '%' . db_like($string) . '%',
      ':lang'   => $language,
      ':types'  => implode(', ', $types),
    ))->fetchAll();
     */

    foreach ($result as $data) {
      if (isset($data->delta)) {
        $key = 'nodeblock.' . $data->delta;
      } else {
        $key = 'nodeblock.' . $data->nid;
      }
      $value = $key . ': ' . check_plain($data->title) . ' [' . $data->language . ']';
      $matches[$value] = $value;
    }
  }

  // TADA!
  drupal_json_output($matches);
}

/**
 * Ensure block exists.
 *
 * @return string
 *   The new or existing block delta in case of success, false else.
 */
function nodeblock_edit_ensure_block($module, $id) {

  $function = $module . '_block_info';

  if (!function_exists($function)) {
    return FALSE;
  }

  $block_list = call_user_func($function);

  if (isset($block_list[$id])) {
    return $id;
  }

  // Nodeblock module integration specifics.
  if ('nodeblock' === $module) {
    if ($node = node_load($id)) {

      try {
        // Ok, got a node! Create the block.
        db_insert('nodeblock')
          ->fields(array(
            'nid'   => $node->nid,
            'delta' => $node->nid,
          ))
          ->execute();

        return $node->nid;

      } catch (Exception $e) {
        return false;
      }
    }
  }

  return FALSE;
}

/**
 * Block delete form.
 */
function nodeblock_edit_page_region_block_delete_form($form, &$form_state, $region_info, $block_info) {

  _nodeblock_edit_form_set_breadcrumb("Delete block on page %page");

  list($region, $theme) = $region_info;
  list($module, $delta) = $block_info;

  $form['region_info'] = array(
    '#type'  => 'value',
    '#value' => $region_info,
  );
  $form['block_info'] = array(
    '#type'  => 'value',
    '#value' => $block_info,
  );

  $form['pages'] = array(
    '#title'         => t("Pages from which this block must disappear"),
    '#type'          => 'checkboxes',
    '#options'       => array(),
    '#required'      => TRUE,
    '#default_value' => array(),
  );

  $options = array();
  $default = array();

  if (isset($_GET['destination'])) {
    // When we have a destination, delete only the current page and aliases.
    $path = drupal_get_normal_path($_GET['destination']);
    $options[$path] = $path;

    // Also ensure we don't have any aliases remaining.
    foreach (nodeblock_edit_path_get_all_aliases($_GET['destination']) as $path) {
      $options[$path] = $path;
    }

    $default = $options;

    $form['pages']['#description'] = '<strong>' . t("Path aliases have been auto-detected from the page you come from.") . '</strong>';
    $form['pages']['#disabled']    = TRUE;
  } else {
    // Load all path this block might be (slow operation).
    $options = db_select('nodeblock_edit_path', 'np')
      ->fields('np', array('path', 'path'))
      ->condition('np.module', $module)
      ->condition('np.delta', $delta)
      ->execute()
      ->fetchAllKeyed();
  }

  // Set the correct values into the field.
  $form['pages']['#options'] = $options;
  $form['pages']['#default_value'] = $default;

  return confirm_form($form, t("Delete the selected blocks"), isset($_GET['destination']) ? $_GET['destination'] : request_path());
}

/**
 * Block delete form submit handler.
 */
function nodeblock_edit_page_region_block_delete_form_submit($form, &$form_state) {

  list($region, $theme) = $form_state['values']['region_info'];
  list($module, $delta) = $form_state['values']['block_info'];
  $pages                = $form_state['values']['pages'];

  if (!empty($pages)) {

    db_delete('nodeblock_edit_path')
      ->condition('module',  $module)
      ->condition('delta', $delta)
      ->condition('region', $region)
      ->condition('path', $pages)
      ->execute();

    drupal_set_message(t("Removed block from the selected pages."));

  } else {

    db_delete('nodeblock_edit_path')
      ->condition('module',  $module)
      ->condition('delta', $delta)
      ->condition('region', $region)
      ->execute();

    drupal_set_message(t("Removed block from all pages."));
  }
}

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
  $trail[] = $item;

  while ($item = _menu_link_find_parent($item)) {
    array_unshift($trail, $item);
  }

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
 * Overview form.
 */
function nodeblock_edit_page_overview_form($form, &$form_state) {

  $options = array();
  foreach (list_themes() as $theme => $info) {
    foreach (system_region_list($theme) as $region => $description) {

      $region_id = $region . '.' . $theme;

      if (!nodeblock_edit_region_is_enabled($region_id)) {
        continue;
      }

      $total_count = db_select('block', 'b')
        ->condition('theme', $theme)
        ->condition('region', $region)
        ->countQuery()
        ->execute()
        ->fetchField();

      $custom_count = db_select('block', 'b')
        ->condition('theme', $theme)
        ->condition('region', $region)
        ->condition('nodeblock_edit_custom', 1)
        ->countQuery()
        ->execute()
        ->fetchField();

      $options[$region_id] = array(
        'theme'  => $info->info['name'],
        'name'   => $description,
        'custom' => $custom_count,
        'total'  => $total_count,
      );
    }
  }

  $form['regions'] = array(
    '#type'    => 'tableselect',
    '#options' => $options,
    '#header'  => array(
      'theme'  => t("Theme"),
      'name'   => t("Region"),
      'custom' => t("Custom block count"),
      'total'  => t("Total block count"),
    ),
  );

  return $form;
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

  $form['nodeblock_edit_enabled_regions'] = array(
    '#type'             => 'checkboxes',
    '#title'            => t("Enabled regions"),
    '#options'          => $options,
    '#default_value'    => variable_get('nodeblock_edit_enabled_regions', array()),
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
 * Add block into region form.
 */
function nodeblock_edit_page_region_add_form($form, &$form_state, $region_info) {
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
  if (($alias = drupal_get_path_alias($path)) && $alias !== $path) {
    $ret[] = $alias;
  }

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
    $aliases = nodeblock_edit_path_get_all_aliases($_GET['destination']);
    if (!in_array($_GET['destination'], $aliases)) {
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
    '#autocomplete_path' => 'admin/structure/region/' . $region_info[0] . '.' . $region_info[1] . '/autocomplete',
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

  $fields = array(
    // We cannot set bid since it's an auto increment value.
    'module'                => $module,
    'delta'                 => $delta,
    'theme'                 => $theme,
    'status'                => 1,
    // FIXME: Default sounds good for testing, but later find a way tied to
    // the UI for leaving the user choosing where to put the weight himself.
    'weight'                => 0,
    'region'                => $region,
    'custom'                => 0,
    // Display on selected pages only.
    'visibility'            => 1,
    'pages'                 => $pages,
    // Leave the original module handling the title itself.
    'title'                 => '',
    // FIXME: Find a way to get back the original (and altered if altered)
    // block cache setting for this block.
    'cache'                 => DRUPAL_NO_CACHE,
    'nodeblock_edit_custom' => 1,
  );

  $existing = db_select('block', 'b')
    ->fields('b')
    ->condition('b.theme',  $theme)
    ->condition('b.module', $module)
    ->condition('b.delta',  $delta)
    ->condition('b.region', $region)
    ->execute()
    ->fetchAssoc();

  if ($existing) {
    if ($existing['status'] && $existing['pages']) {
      $pages           = array_unique(explode("\n", $fields['pages'] . "\n" . $existing['pages']));
      $pages           = array_filter(array_values($pages));
      $fields['pages'] = implode("\n", $pages);
    }
    $fields['weight'] = $existing['weight'];
    $fields['bid']    = $existing['bid'];
  }

  // Add or ensure the block to be into the {blocks} table, then enable it
  // and clear associated cache.
  if ($existing) {
    db_merge('block')
      ->key(array(
        'bid' => $existing['bid'],
      ))
      ->fields($fields)
      ->execute();
  } else {
    db_insert('block')
      ->fields($fields)
      ->execute();
  }

  cache_clear_all();

  if ($existing) {
    drupal_set_message(t("The already existing block has been updated to be displayed in this page."));
  } else {
    drupal_set_message(t("The new block has been added on this page."));
  }
}

/**
 * Edit or move block into region.
 */
function nodeblock_edit_page_region_block_form($form, &$form_state, $region_info, $block_info) {

  _nodeblock_edit_form_set_breadcrumb("Move blocks on page %page", array(), "Move blocks");

  list($module, $id)    = $block_info;
  list($region, $theme) = $region_info;

  $form['#tree'] = TRUE;

  // Display a nice table with all other blocks, and allow to change the
  // weight of the selected one (and others as we speak).
  $block_info = db_select('block', 'b')
    ->fields('b')
    ->condition('b.theme',  $theme)
    ->condition('b.region', $region)
    ->condition('b.status', 1)
    ->orderBy('b.weight')
    ->orderBy('b.module')
    ->addTag('block_load')
    ->addTag('translatable')
    ->execute()
    ->fetchAllAssoc('bid');

  drupal_alter('nodeblock_edit', $block_info);
  drupal_alter('block_list', $block_info);

  foreach ($block_info as $block) {
    $function = $block->module . '_block_info';
    if (function_exists($function)) {
      $info = $function();
      if (isset($info[$block->delta])) {
        $block->info = $info[$block->delta]['info'];
      }
    }

    if (!isset($block->info)) {
      // Ugly fallback, sorry.
      $block->info = '<em>' . $block->module . '-' . $block->delta . '</em>';
    }
  }

  $modules = module_list();

  foreach ($block_info as $block) {
    $id = $block_id = $block->module . '.' . $block->delta;

    $form['blocks'][$id]['weight'] = array(
      '#type'          => 'weight',
      '#title'         => t('Weight for @title', array('@title' => $block->info)),
      '#title_display' => 'invisible',
      '#default_value' => $block->weight,
      '#disabled'      => !$block->nodeblock_edit_custom,
    );
    $form['blocks'][$id]['info'] = array(
      '#markup'        => $block->info,
    );
    $form['blocks'][$id]['module'] = array(
      '#markup'        => $modules[$block->module],
    );
    $form['blocks'][$id]['custom'] = array(
      '#markup'        => $block->nodeblock_edit_custom ? t("Yes") : '',
    );
  }

  $form['block_info'] = array(
    '#type'  => 'value',
    '#value' => $block_info,
  );
  $form['region_info'] = array(
    '#type'  => 'value',
    '#value' => $region_info,
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
        drupal_render($form['blocks'][$id]['custom']),
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
      t("Is custom"),
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
    // FIXME: Totally unacceptable query if there was millions of nodes.
    $query = "
      SELECT * FROM (
        SELECT n.nid, n.title, n.language, b.delta, n.tnid
          FROM {node} n
          LEFT JOIN {nodeblock} b ON b.nid = n.nid
          WHERE LOWER(n.title) LIKE :string
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
    ))->fetchAll();

    foreach ($result as $data) {
      if ($data->delta) {
        $key = 'nodeblock.' . $data->delta;
      } else {
        $key = 'nodeblock.' . $data->nid;
      }
      $matches[$key] = check_plain($data->title) . ' [' . $data->language . ']';
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

  $existing = db_select('block', 'b')
    ->fields('b')
    ->condition('b.theme',  $theme)
    ->condition('b.module', $module)
    ->condition('b.delta',  $delta)
    ->execute()
    ->fetchAssoc();

  $form['region_info'] = array(
    '#type'  => 'value',
    '#value' => $region_info,
  );
  $form['block_info'] = array(
    '#type'  => 'value',
    '#value' => $block_info,
  );

  if (!empty($existing['pages'])) {
    $options = array();
    foreach (explode("\n", $existing['pages']) as $path) {
      $options[$path] = $path;
    }
    $form['pages'] = array(
      '#title'         => t("Pages from which this block must disappear"),
      '#type'          => 'checkboxes',
      '#options'       => $options,
      '#required'      => TRUE,
      '#default_value' => array(),
    );

    if (isset($_GET['destination'])) {
      // When we have a destination, delete only the current page and aliases.
      $form['pages']['#default_value'][$_GET['destination']] = $_GET['destination'];

      foreach (nodeblock_edit_path_get_all_aliases($_GET['destination']) as $path) {
        $form['pages']['#default_value'][$path] = $path;
      }

      $form['pages']['#description'] = '<strong>' . t("Path aliases have been auto-detected from the page you come from.") . '</strong>';
      $form['pages']['#disabled']    = TRUE;

    } else {
      // Per default, suppress all.
      $form['pages']['#default_value'] = $options;
    }
  } else {
    $form['pages'] = array(
      '#type'  => 'value',
      '#value' => array(),
    );
  }

  return confirm_form($form, t("Delete the selected blocks"), isset($_GET['destination']) ? $_GET['destination'] : request_path());
}

function nodeblock_edit_page_region_block_delete_form_submit($form, &$form_state) {

  list($region, $theme) = $form_state['values']['region_info'];
  list($module, $delta) = $form_state['values']['block_info'];
  $pages                = $form_state['values']['pages'];

  if (!empty($pages)) {

    $existing = db_select('block', 'b')
      ->fields('b')
      ->condition('b.theme',  $theme)
      ->condition('b.module', $module)
      ->condition('b.delta',  $delta)
      ->execute()
      ->fetchAssoc();

    $original_pages = array_unique(explode("\n", $existing['pages']));
    $pages          = array_filter(array_values($pages));
    $diff           = array_diff($original_pages, $pages);

    if (empty($diff)) {
      // Fully delete block.
      db_delete('block')
        ->condition('theme',  $theme)
        ->condition('module', $module)
        ->condition('delta',  $delta)
        ->execute();

      drupal_set_message(t("Removed block from all pages."));

    } else {
      // Update the block with only diffed remaining pages.
      db_update('block')
        ->fields(array(
          'pages' => implode("\n", $diff)
        ))
        ->condition('theme',  $theme)
        ->condition('module', $module)
        ->condition('delta',  $delta)
        ->execute();

      drupal_set_message(t("Removed block from the selected pages."));
    }
  } else {
    // Fully delete block.
    db_delete('block')
      ->condition('theme',  $theme)
      ->condition('module', $module)
      ->condition('delta',  $delta)
      ->execute();

    drupal_set_message(t("Removed block from all pages."));
  }
}
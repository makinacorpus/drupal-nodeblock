<?php

/**
 * @file
 * Da user forms.
 */

/**
 * Tell if the block delta exists.
 *
 * @param string $delta
 *   Delta to check upon.
 */
function nodeblock_delta_exists($delta) {
  return (bool)db_query("SELECT 1 FROM {nodeblock} WHERE delta = :delta", array(':delta' => $delta))->fetchField();
}

/**
 * Create new block for a node form.
 */
function nodeblock_create_form($form, &$form_state, $node) {

  // Select item if exists.
  $values = db_select('nodeblock', 'nb')
    ->fields('nb')
    ->condition('nb.nid', $node->nid)
    ->execute()
    ->fetchObject();

  // Ensure defaults.
  if ($values) {
    $exists = TRUE;
  } else {
    $values = (object)array(
      'nid'   => $node->nid,
      'info'  => check_plain($node->title),
      'title' => NULL,
      'delta' => NULL,
    );
    $exists = FALSE;
  }

  $form['exists'] = array(
    '#type'  => 'value',
    '#value' => $exists,
  );
  $form['nid'] = array(
    '#type'  => 'value',
    '#value' => $values->nid,
  );

  $form['info'] = array(
    '#title'         => t("Administrative title"),
    '#type'          => 'textfield',
    '#required'      => TRUE,
    '#default_value' => $values->info,
    '#description'   => t("This name will only be displayed in administration screens"),
  );

  $form['delta'] = array(
    '#type'          => 'machine_name',
    '#title'         => t("Identifier"),
    '#required'      => TRUE,
    '#machine_name'  => array(
      'exists'       => 'nodeblock_delta_exists',
      'source'       => array('info'),
      'label'        => t("Identifier"),
    ),
    // 32 is the 'delta' column max length.
    '#maxlength'     => 32,
    '#default_value' => $values->delta,
    '#disabled'      => $exists,
  );

  $form['title'] = array(
    '#title'         => t("Block title"),
    '#type'          => 'textfield',
    '#required'      => FALSE,
    '#default_value' => $values->title,
    '#description'   => t("Block title for override and use with context, defaults to the node title. Write <em>&lt;none&gt;</em> for explicit no title."),
  );

  $form['actions'] = array(
    '#type'  => 'actions',
    'submit' => array(
      '#type'  => 'submit',
      '#value' => $exists ? t("Update") : t("Create"),
    ),
  );

  return $form;
}

/**
 * Create new block for a node form validate handler.
 */
function nodeblock_create_form_validate($form, &$form_state) {

  $nid    = $form_state['values']['nid'];
  $exists = $form_state['values']['exists'];
  $delta  = $form_state['values']['delta'];

  if (!$node = node_load($nid)) {
    form_set_error(NULL, t("This node does not exists"));
  } else if (!$exists && nodeblock_delta_exists($delta)) {
    form_set_error('delta', t("This delta already exists"));
  }
}

/**
 * Create new block for a node form submit handler.
 */
function nodeblock_create_form_submit($form, &$form_state) {

  $nid    = $form_state['values']['nid'];
  $exists = $form_state['values']['exists'];
  $delta  = $form_state['values']['delta'];
  $info   = $form_state['values']['info'];
  $title  = $form_state['values']['title'];

  db_merge('nodeblock')
    ->key(array(
      'nid' => (int)$nid,
    ))
    ->fields(array(
      'nid'   => (int)$nid,
      'delta' => $delta,
      'info'  => $info,
      'title' => $title,
    ))
    ->execute();

  if ($exists) {
    drupal_set_message(t("New block created with delta %delta", array(
      '%delta' => $delta,
    )));
  } else {
    drupal_set_message(t("Block info has been updated"));
  }

  $form_state['redirect'] = 'node/' . $nid;
}

/**
 * Delete the node block confim form.
 */
function nodeblock_remove_form($form, &$form_state, $node) {

  $form['nid'] = array(
    '#type'  => 'value',
    '#value' => $node->nid,
  );

  return confirm_form($form, t("Delete this node's block?"), 'node/' . $node->nid);
}

/**
 * Delete the node block confim form validate handler.
 */
function nodeblock_remove_form_validate($form, &$form_state) {

  $nid = $form_state['values']['nid'];

  if (!$node = node_load($nid)) {
    form_set_error(NULL, t("This node does not exists"));
  } else if (!nodeblock_node_has_block($node)) {
    form_set_error(NULL, t("This node already has a block"));
  }
}

/**
 * Delete the node block confim form submit handler.
 */
function nodeblock_remove_form_submit($form, &$form_state) {

  $nid = $form_state['values']['nid'];

  db_delete('nodeblock')
    ->condition('nid', $nid)
    ->execute();

  drupal_set_message(t("Block deleted"));

  $form_state['redirect'] = 'node/' . $nid;
}


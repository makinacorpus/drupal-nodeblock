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

  $form['nid'] = array(
    '#type'  => 'value',
    '#value' => $node->nid,
  );

  $form['title'] = array(
    '#title'         => t("Administrative title"),
    '#type'          => 'textfield',
    '#required'      => TRUE,
    '#default_value' => check_plain($node->title),
    '#description'   => t("This name will only be displayed in administration screens"),
  );

  $form['delta'] = array(
    '#type'         => 'machine_name',
    '#title'        => t("Identifier"),
    '#required'     => TRUE,
    '#machine_name' => array(
      'exists'      => 'nodeblock_delta_exists',
      'source'      => array('title'),
      'label'       => t("Identifier"),
    ),
  );

  $form['actions'] = array(
    '#type'  => 'actions',
    'submit' => array(
      '#type'  => 'submit',
      '#value' => t("Create"),
    ),
  );

  return $form;
}

/**
 * Create new block for a node form validate handler.
 */
function nodeblock_create_form_validate($form, &$form_state) {

  $nid   = $form_state['values']['nid'];
  $delta = $form_state['values']['delta'];

  if (!$node = node_load($nid)) {
    form_set_error(NULL, t("This node does not exists"));
  } else if (nodeblock_node_has_block($node)) {
    form_set_error(NULL, t("This node already has a block"));
  } else if (nodeblock_delta_exists($delta)) {
    form_set_error('delta', t("This delta already exists"));
  }
}

/**
 * Create new block for a node form submit handler.
 */
function nodeblock_create_form_submit($form, &$form_state) {

  $nid   = $form_state['values']['nid'];
  $delta = $form_state['values']['delta'];
  $title = $form_state['values']['title'];

  db_insert(NODEBLOCK_TABLENAME)
    ->fields(array(
      'nid'   => (int)$nid,
      'delta' => check_plain($delta),
      'title' => check_plain($title),
    ))
    ->execute();

  drupal_set_message(t("New block created"));

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

  db_delete(NODEBLOCK_TABLENAME)
    ->condition('nid', $nid)
    ->execute();

  drupal_set_message(t("Block deleted"));

  $form_state['redirect'] = 'node/' . $nid;
}


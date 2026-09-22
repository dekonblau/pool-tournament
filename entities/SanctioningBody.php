<?php
/**
 * @file
 * SanctioningBody entity class.
 */

class SanctioningBody extends Entity {
  public $sanctioning_body_id;
  public $bundle;
  public $label;
  public $description;
  public $rulebook_url;
  public $created;

  public function __construct(array $values = array(), $entity_type = 'sanctioning_body') {
    parent::__construct($values, $entity_type);
    if (empty($this->created)) {
      $this->created = REQUEST_TIME;
    }
  }

  public function entityType() { return 'sanctioning_body'; }
  public function label() { return $this->label; }
  public function id() { return isset($this->sanctioning_body_id) ? $this->sanctioning_body_id : NULL; }
  public function bundle() { return $this->bundle; }
  public function uri() {
    return array('path' => 'sanctioning-body/' . $this->id(), 'options' => array());
  }
}

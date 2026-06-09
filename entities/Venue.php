<?php
/**
 * @file
 * Venue entity class.
 */

class Venue extends Entity {
  public $venue_id;
  public $bundle;
  public $label;
  public $address;
  public $city;
  public $state;
  public $phone;
  public $created;

  public function __construct(array $values = array(), $entity_type = 'venue') {
    parent::__construct($values, $entity_type);
    if (empty($this->created)) {
      $this->created = REQUEST_TIME;
    }
  }

  public function entityType() { return 'venue'; }
  public function label() { return $this->label; }
  public function id() { return isset($this->venue_id) ? $this->venue_id : NULL; }
  public function bundle() { return $this->bundle; }
  public function uri() {
    return array('path' => 'venue/' . $this->id(), 'options' => array());
  }
}

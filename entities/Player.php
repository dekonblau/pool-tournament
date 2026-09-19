<?php
/**
 * @file
 * Player entity class.
 */

class Player extends Entity {
  public $player_id;
  public $bundle;
  public $label;
  public $uid;
  public $member_id;
  public $membership_id;
  public $fargo_rating;
  public $robustness;
  public $location;
  public $created;
  public $status;

  public function __construct(array $values = array(), $entity_type = 'player') {
    parent::__construct($values, $entity_type);
    if (empty($this->created)) {
      $this->created = REQUEST_TIME;
    }
    if (!isset($this->status)) {
      $this->status = 1;
    }
  }

  public function entityType() { return 'player'; }
  public function label() { return $this->label; }
  public function id() { return isset($this->player_id) ? $this->player_id : NULL; }
  public function bundle() { return $this->bundle; }
  public function uri() {
    return array('path' => 'player/' . $this->id(), 'options' => array());
  }
}

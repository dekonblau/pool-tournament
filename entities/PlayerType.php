<?php
/**
 * @file
 * PlayerType entity class.
 */

class PlayerType extends Entity {
  public $player_type_id;
  public $bundle;
  public $label;
  public $description;
  public $players_per_side;
  public $created;

  public function __construct(array $values = array(), $entity_type = 'player_type') {
    parent::__construct($values, $entity_type);
    if (empty($this->created)) {
      $this->created = REQUEST_TIME;
    }
    if (!isset($this->players_per_side)) {
      $this->players_per_side = 1;
    }
  }

  public function entityType() { return 'player_type'; }
  public function label() { return $this->label; }
  public function id() { return isset($this->player_type_id) ? $this->player_type_id : NULL; }
  public function bundle() { return $this->bundle; }
  public function uri() {
    return array('path' => 'player-type/' . $this->id(), 'options' => array());
  }
}

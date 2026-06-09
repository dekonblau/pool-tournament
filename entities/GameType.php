<?php
/**
 * @file
 * GameType entity class.
 */

class GameType extends Entity {
  public $game_type_id;
  public $bundle;
  public $label;
  public $description;
  public $created;

  public function __construct(array $values = array(), $entity_type = 'game_type') {
    parent::__construct($values, $entity_type);
    if (empty($this->created)) {
      $this->created = REQUEST_TIME;
    }
  }

  public function entityType() { return 'game_type'; }
  public function label() { return $this->label; }
  public function id() { return isset($this->game_type_id) ? $this->game_type_id : NULL; }
  public function bundle() { return $this->bundle; }
  public function uri() {
    return array('path' => 'game-type/' . $this->id(), 'options' => array());
  }
}

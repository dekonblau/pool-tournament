<?php
/**
 * @file
 * TournamentType entity class.
 */

class TournamentType extends Entity {
  public $tournament_type_id;
  public $bundle;
  public $label;
  public $description;
  public $created;

  public function __construct(array $values = array(), $entity_type = 'tournament_type') {
    parent::__construct($values, $entity_type);
    if (empty($this->created)) {
      $this->created = REQUEST_TIME;
    }
  }

  public function entityType() { return 'tournament_type'; }
  public function label() { return $this->label; }
  public function id() { return isset($this->tournament_type_id) ? $this->tournament_type_id : NULL; }
  public function bundle() { return $this->bundle; }
  public function uri() {
    return array('path' => 'tournament-type/' . $this->id(), 'options' => array());
  }
}

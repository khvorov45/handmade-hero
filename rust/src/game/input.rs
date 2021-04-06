#[derive(Debug, PartialEq, Clone, Copy, Default)]
pub struct Controller {
    pub analogs: Analogs,
    pub buttons: Buttons,
}

#[derive(Debug, PartialEq, Clone, Copy, Default)]
pub struct Analogs {
    pub left: Analog,
    pub right: Analog,
}

#[derive(Debug, PartialEq, Clone, Copy, Default)]
pub struct Analog {
    pub x: Axis,
    pub y: Axis,
}

#[derive(Debug, PartialEq, Clone, Copy, Default)]
pub struct Axis {
    pub start: f32,
    pub end: f32,
    pub min: f32,
    pub max: f32,
}

#[derive(Debug, PartialEq, Clone, Copy, Default)]
pub struct Buttons {
    pub up: Button,
    pub down: Button,
    pub left: Button,
    pub right: Button,
    pub left_shoulder: Button,
    pub right_shoulder: Button,
}

#[derive(Debug, PartialEq, Clone, Copy, Default)]
pub struct Button {
    pub ended_down: bool,
}

#[implement(ITfTextInputProcessor, ITfKeyEventSink)]
struct TextService {
    composition_buffer: String,
}

impl TextService {}

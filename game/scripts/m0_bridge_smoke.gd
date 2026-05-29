extends Control
## M0 smoke test for the Godot <-> custom Vulkan RT presentation bridge.
##
## Brings up a PathTracedViewport (from the `pathtracer` GDExtension), reports
## whether a ray-tracing-capable device was created, and displays the shared
## output texture fullscreen with a status label on top. Once the bridge renders
## an animated test pattern, the M0 deliverable is met (see docs/ROADMAP.md).

@onready var _viewport: PathTracedViewport = $PathTracedViewport
@onready var _output: TextureRect = $Output
@onready var _status: Label = $Status


func _ready() -> void:
	if not _viewport.is_ray_tracing_available():
		_status.text = "M0: ray tracing NOT available — see Output log."
		return

	var tex := _viewport.get_output_texture()
	if tex == null:
		_status.text = "M0: RT device up, but presentation bridge has no texture yet."
		return

	_output.texture = tex
	_status.text = "M0: ray tracing available — compositing path-traced output."


func _process(_delta: float) -> void:
	# Keep the fullscreen output sized to the window.
	_output.size = get_viewport_rect().size

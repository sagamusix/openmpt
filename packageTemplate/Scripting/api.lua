--------------------------------------------------------------------------------
--  OpenMPT Scripting API
--
--  Naming conventions:
--  * Namespaces and enumerations are lowercase
--  * Class names CamelCase.
--  * Constants are ALL-CAPS
--
--  All strings are UTF-8.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- openmpt
--------------------------------------------------------------------------------

-- TODO
openmpt.register_callback()

-- Creates a new song. The song format (one of openmpt.Song.format) can be
-- specified optionally. If it is not specified, the default settings are used.
openmpt.new_song([format]) -> [openmpt.Song object or nil]

-- Returns the last focussed song
openmpt.active_song() -> [openmpt.Song object or nil]

--------------------------------------------------------------------------------
-- openmpt.util
--------------------------------------------------------------------------------

-- Converts a linear volume level to decibel (0 = -inf dB, 1 = 0dB)
openmpt.util.lin2db(linear) -> [number]

-- Converts a volume level in decibel to linear (-inf dB = 0, 0 dB = 1)
openmpt.util.db2lin(decibel) -> [number]

-- TODO
openmpt.util.fft()

-- TODO
openmpt.util.ifft()

-- Evaluates the expression string and returns the result.
-- Example: openmpt.util.eval("return 3 + 4") returns 7. 
openmpt.util.eval(expression) -> [object]

--------------------------------------------------------------------------------
-- openmpt.app
--------------------------------------------------------------------------------

-- The Scripting API version of this OpenMPT instance, currently 1.0
-- The API version follows semantic versioning (https://semver.org/).
-- Practically, this means that minor increments (e.g. 1.0 -> 1.1) may add new
-- functionality to the API. Changes that are not backwards-compatible, such as
-- removal or renaming of an API element are only done in major increments
-- (e.g. 1.9 -> 2.0)
openmpt.app.API_VERSION -> [read-only, number]

-- The OpenMPT version
-- Example: "1.29.01.00"
openmpt.app.OPENMPT_VERSION -> [read-only, string]

-- Opens the specified song file(s)
-- Returns the opened songs.
-- Example: openmpt.app.load("c:/module1.it", "c:/module2.s3m")
openmpt.app.load(filename1 [, filename2...]) -> [{openmpt.Song object}]

-- Returns a list of currently open songs.
openmpt.app.open_songs() -> [{openmpt.Song object}]

-- Returns a list of recently opened files.
openmpt.app.recent_songs() -> [{string}]

-- Like print(), but shows a message box.
openmpt.app.show_message(obj1 [, obj2...])

-- Registers keyboard shortcut to execute function func.
-- Returns the shortcut ID which is passed as a parameter to func and can be
-- used to remove the shortcut using openmpt.app.remove_shortcut.
-- Modifier keys are: SHIFT, CTRL, ALT, WIN, CC, RSHIFT, RCTRL, RALT
openmpt.app.register_shortcut(func, key...) -> [number]

-- Remove a shortcut previously registered using openmpt.app.register_shortcut.
openmpt.app.remove_shortcut(id)

-- Returns a list of all known plugins.
openmpt.app.registered_plugins() -> [{openmpt.Plugin object}]

--------------------------------------------------------------------------------
-- openmpt.Song
--------------------------------------------------------------------------------

-- Saves the song. If no filename is provided, the current filename is used.
openmpt.Song:save([filename])
-- Closes the song. Note that this call may fail if there are any open dialogs.
openmpt.Song:close()
-- True if the song has been modified since the last save.
openmpt.Song.modified -> [read-only, boolean]
-- The song name.
openmpt.Song.name -> [string]
-- The song artist.
openmpt.Song.artist -> [string]
openmpt.Song.type -> [openmpt.Song.format]
openmpt.Song.channels -> [{openmpt.Song.Channel}]
openmpt.Song.sample_volume -> [number]
openmpt.Song.plugin_volume -> [number]
openmpt.Song.initial_global_volume -> [number]
openmpt.Song.current_global_volume -> [number]
openmpt.Song.initial_tempo -> [number]
openmpt.Song.current_tempo -> [number]
openmpt.Song.initial_speed -> [number]
openmpt.Song.current_speed -> [number]
openmpt.Song.time_signature -> [openmpt.Song.TimeSignature]
openmpt.Song.samples -> [{openmpt.Song.Sample}]
openmpt.Song.instruments -> [{openmpt.Song.Instrument}]
openmpt.Song.patterns -> [{openmpt.Song.Pattern}]
openmpt.Song.plugins -> [{openmpt.Song.Plugin}]
-- Returns the length of all detected subsongs. every entry has the following
-- properties: duration (in seconds), start_sequence, start_order, start_row
openmpt.Song.length -> [{table}]
openmpt.Song:focus()

--------------------------------------------------------------------------------
-- openmpt.Song.format
--------------------------------------------------------------------------------

openmpt.Song.format.MOD
openmpt.Song.format.XM
openmpt.Song.format.S3M
openmpt.Song.format.IT
openmpt.Song.format.MPTM

--------------------------------------------------------------------------------
-- openmpt.Song.transpose
--------------------------------------------------------------------------------

openmpt.Song.transpose.OCTAVE_DOWN
openmpt.Song.transpose.OCTAVE_UP

--------------------------------------------------------------------------------
-- openmpt.Song.Channel
--------------------------------------------------------------------------------

-- The index of the channel, e.g. 1 for the first channel.
openmpt.Song.Channel.id -> [read-only, number]
-- The channel name.
openmpt.Song.Channel.name -> [string]

--------------------------------------------------------------------------------
-- openmpt.Song.Sample
--------------------------------------------------------------------------------

openmpt.Song.Sample
				//, "save"
				//, "load"
				//, "external"
				//, "modified" for external samples
				//, "path"
-- The index of the sample, e.g. 1 for the first sample.
openmpt.Song.Sample.id -> [read-only, number]
-- The sample name.
openmpt.Song.Sample.name -> [string]
-- The sample filename.
openmpt.Song.Sample.filename -> [string]
-- The sample length in frames (sample points).
openmpt.Song.Sample.length -> [number]
-- The sample depth in bits, either 8 or 16. Setting it to any other value will
-- cause an error.
openmpt.Song.Sample.bits_per_sample -> [number]
-- The number of sample channels, either 1 (mono) or 2 (stereo). Setting it to
-- any other value will cause an error.
openmpt.Song.Sample.channels -> [number]
openmpt.Song.Sample.loop -> [openmpt.Song.SampleLoop object]
openmpt.Song.Sample.sustain_loop -> [openmpt.Song.SampleLoop object]
-- The sampling frequency of a middle-c.
openmpt.Song.Sample.frequency -> [number]
-- The panning position, ranging from -1.0 (left) to 1.0 (right) or nil if no panning is enforced
openmpt.Song.Sample.panning -> [number or nil]
-- The default sample volume in range [0.0, 1.0]
openmpt.Song.Sample.volume -> [number]
-- The global volume in range [0.0, 1.0]
openmpt.Song.Sample.global_volume -> [number]

			, "normalize", &Normalize
			//, "remove_dc_offset"

-- Plays the given note of the sample, at a volume in [0.0, 1.0], at a panning position in [-1.0, 1.0]
-- Returns the channel on which the note is played on success, or nil otherwise 
openmpt.Song.Sample:play(note [, volume [, panning]])

-- Stops the note playing on the given channel (e.g. returned by play())
openmpt.Song.Sample:stop(channel)

-- Amplifies the sample data.
-- Returns true on success.
openmpt.Song.Sample:amplify(amplify_start [, amplify_end [, start[, end]]) -> [boolean]

-- Changes the stereo separation of the sample data by a factor between 0 (make mono) and 2 (double the separation)
-- Returns true on success.
openmpt.Song.Sample:change_stereo_separation(factor [, start[, end]]) -> [boolean]

-- Reverses the sample data.
-- Returns true on success.
openmpt.Song.Sample:reverse([start[, end]]) -> [boolean]

-- Inverts the sample data.
-- Returns true on success.
openmpt.Song.Sample:invert([start[, end]]) -> [boolean]

-- Applies a theoretical signed<->unsigned conversion to the sample data.
-- Returns true on success.
openmpt.Song.Sample:unsign([start[, end]]) -> [boolean]

-- Silences part of the sample.
-- Returns true on success.
openmpt.Song.Sample:unsign([start[, end]]) -> [boolean]

			//, "resample"
			//, "autotune"



--------------------------------------------------------------------------------
-- openmpt.Song.loopType
--------------------------------------------------------------------------------

openmpt.Song.loopType.OFF
openmpt.Song.loopType.FORWARD
openmpt.Song.loopType.PINGPONG

--------------------------------------------------------------------------------
-- openmpt.Song.SampleLoop
--------------------------------------------------------------------------------

-- The loop start and end point. The end point is exclusive.
openmpt.Song.SampleLoop.range -> [array of two numbers]
-- The loop type (off, forward, ping-pong)
openmpt.Song.SampleLoop.type -> [openmpt.Song.loopType]

-- Crossfades the loop for the given duration in seconds, using the specified fade law (a value in the range [0.0 = constant volume, 1.0 = constant power]), optionally fading back to the original sample data after the loop end (true by default).
-- Returns true on success.
openmpt.Song.SampleLoop:crossfade(duration[, fade_law[, fade_after_loop]]) -> [boolean]

--------------------------------------------------------------------------------
-- openmpt.Song.Instrument
--------------------------------------------------------------------------------

-- The index of the instrument, e.g. 1 for the first instrument.
openmpt.Song.Instrument.id -> [read-only, number]
-- The instrument name.
openmpt.Song.Instrument.name -> [string]
-- The instrument filename.
openmpt.Song.Instrument.filename -> [string]
-- The panning position, ranging from -1.0 (left) to 1.0 (right) or nil if no panning is enforced
openmpt.Song.Instrument.panning -> [number or nil]
-- The global volume in range [0.0, 1.0]
openmpt.Song.Instrument.global_volume -> [number]
-- A list of all referenced samples
openmpt.Song.Instrument.samples -> [{openmpt.Song.Sample}]

-- Plays the given note of the instrument, at a volume in [0.0, 1.0], at a panning position in [-1.0, 1.0]
-- Returns the channel on which the note is played on success, or nil otherwise 
openmpt.Song.Instrument:play(note [, volume [, panning]])

-- Stops the note playing on the given channel (e.g. returned by play())
openmpt.Song.Instrument:stop(channel)

-- The index of the pattern e.g. 0 for the first pattern.
openmpt.Song.Pattern.id -> [read-only, number]
-- The pattern name.
openmpt.Song.Pattern.name -> [string]
-- The number of rows in the pattern.
openmpt.Song.Pattern.rows -> [number]
openmpt.Song.Pattern.data -> [{openmpt.Song.PatternCell}]

-- The time signature of the pattern, or nil if the pattern does not override
-- the global time signature.
openmpt.Song.Pattern.time_signature -> [openmpt.Song.TimeSignature or nil]

-- Transpose all notes in the pattern by the given amount, which can also be
-- one of the special values openmpt.Song.transpose.OCTAVE_DOWN and openmpt.Song.transpose.OCTAVE_UP
openmpt.Song.Pattern:transpose(steps)


openmpt.Song.Plugin.name -> [string]
openmpt.Song.Plugin.library_name -> [read-only, string]
openmpt.Song.Plugin.is_loaded -> [read-only, boolean]
openmpt.Song.Plugin.index -> [read-only, number]
openmpt.Song.Plugin.parameters -> [{openmpt.Song.PluginParameter}]
				//, "toggle_editor()"
openmpt.Song.Plugin.master -> [boolean]
				//, "drymix"
-- The bypass state of the plugin
openmpt.Song.Plugin.bypass -> [boolean]
				//, "expand"
-- The dry/wet ratio, ranging from 0 (100% wet / 0% dry) to 1 (0% wet / 100% dry)
openmpt.Song.Plugin.dry_wet_ratio -> [number]
				//, "mixmode"
-- Plugin post-gain
openmpt.Song.Plugin.gain -> [number]
openmpt.Song.Plugin.output -> [openmpt.Song.Plugin or number]

openmpt.Song.PluginParameter.name -> [read-only, string]
openmpt.Song.PluginParameter.value -> [number]

--------------------------------------------------------------------------------
-- openmpt.fadelaw
--------------------------------------------------------------------------------

openmpt.fadelaw.LINEAR
openmpt.fadelaw.EXPONENTIAL
openmpt.fadelaw.SQUARE_ROOT
openmpt.fadelaw.LOGARITHMIC
openmpt.fadelaw.QUARTER_SINE
openmpt.fadelaw.HALF_SINE

--------------------------------------------------------------------------------
-- MIDI
--------------------------------------------------------------------------------

-- CC enum?

openmpt.Midi.CC.channel
openmpt.Midi.CC.controller
openmpt.Midi.CC.value

openmpt.Midi.NoteOn.channel
openmpt.Midi.NoteOn.note
openmpt.Midi.NoteOn.velocity

openmpt.Midi.NoteOff.channel
openmpt.Midi.NoteOff.note
openmpt.Midi.NoteOff.velocity

openmpt.Midi.PitchBend.channel
openmpt.Midi.PitchBend.amount
--------------------------------------------------------------------------------
--  OpenMPT Scripting API
--
--  Naming conventions:
--  * Namespaces and enumerations are lowercase
--  * Class names CamelCase.
--  * Constants are ALL_CAPS
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

-- Translates a cutoff value to frequency in Hz for the given song.
openmpt.util.cutoff2frequency(song, cutoff) -> [number]

-- Translates a frequency value to cutoff for the given song.
openmpt.util.frequency2cutoff(song, frequency) -> [number]

-- Computes FFT of provided real and optional imaginary signals.
-- The result is in bit-reversed order.
openmpt.util.fft(real [, imaginary])

-- Computes IFFT of provided real and optional imaginary signals.
-- The input is expected to be in bit-reversed order.
openmpt.util.ifft(real [, imaginary])

-- Evaluates the expression string as Lua code and returns the result.
-- Example: openmpt.util.eval("return 3 + 4") returns 7. 
openmpt.util.eval(expression) -> [object]

-- Returns the contents of a Lua table as a string.
openmpt.util.dump -> [string]

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
-- Example: "1.31.01.00"
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
-- Regular keys are: A..Z, 0..9, NUM0..NUM9, F1..F24, CLEAR, RETURN, MENU, PAUSE,
-- CAPITAL, KANA, HANGUL, JUNJA, FINAL, HANJA, KANJI, ESC, CONVERT, NONCONVERT,
-- ACCEPT, MODECHANGE, " ", PREV, NEXT, END, HOME, LEFT, UP, RIGHT, DOWN,
-- SELECT, PRINT, EXECUTE, SNAPSHOT, INSERT, DELETE, HELP, APPS, SLEEP, NUM*,
-- NUM+, NUM-, NUM., NUM/
openmpt.app.register_shortcut(func, key...) -> [number]

-- Remove a shortcut previously registered using openmpt.app.register_shortcut.
-- Return true if the shortcut ID was found and removed.
openmpt.app.remove_shortcut(id) -> [boolean]

-- Returns a list of all known plugins.
openmpt.app.registered_plugins() -> [{openmpt.Plugin object}]

-- Registers a MIDI filter function that is applied to incoming MIDI messages.
-- Returns the callback ID which is passed as a parameter to func and can be
-- used to remove the filter using openmpt.app.remove_midi_filter.
-- The function receives an array of bytes and is expected to return a message
-- in the same format. Returning an empty array drops the message, any other
-- value modifies the received message.
-- The second parameter to the function is the callback ID, the third parameter
-- identifies the device from which the message was received.
openmpt.app.register_midi_filter(func) -> [number]

-- Remove a filter previously registered using openmpt.app.register_midi_filter.
-- Return true if the callback ID was found and removed.
openmpt.app.remove_midi_filter(id) -> [boolean]

-- Retrieves value for the given key within the given section.
-- If the key is not found in the section, the optional default value is returned.
openmpt.app.get_state(section, key [, default_value]) -> [string]

-- Set the value for the given key within the given section.
-- If the optional parameter persist is set to true, the value will be remembered
-- even after restarting OpenMPT.
openmpt.app.set_state(section, key, value [, persist])

-- Deletes the value for the given key within the given section.
openmpt.app.delete_state(section, key)

--------------------------------------------------------------------------------
-- openmpt.Song
--------------------------------------------------------------------------------

-- Saves the song. If no filename is provided, the current filename is used.
openmpt.Song:save([filename])
-- Closes the song. Note that this call may fail if there are any open dialogs.
openmpt.Song:close()
-- True if the song has been modified since the last save.
openmpt.Song.modified -> [read-only, boolean]
-- The on-disk path of the file, may be empty if the file has not been saved yet.
openmpt.Song.path -> [read-only, string]
-- The song name.
openmpt.Song.name -> [string]
-- The song artist.
openmpt.Song.artist -> [string]
-- The song format (MOD, XM, S3M, IT, MPTM).
openmpt.Song.type -> [openmpt.Song.format]
-- An array containing the information about all channels in the song.
openmpt.Song.channels -> [{openmpt.Song.Channel}]
openmpt.Song.sample_volume -> [number]
openmpt.Song.plugin_volume -> [number]
openmpt.Song.initial_global_volume -> [number]
openmpt.Song.current_global_volume -> [number]
openmpt.Song.current_tempo -> [number]
openmpt.Song.current_speed -> [number]
openmpt.Song.time_signature -> [openmpt.Song.TimeSignature]
openmpt.Song.samples -> [{openmpt.Song.Sample}]
openmpt.Song.instruments -> [{openmpt.Song.Instrument}]
openmpt.Song.patterns -> [{openmpt.Song.Pattern}]
openmpt.Song.plugins -> [{openmpt.Song.Plugin}]
openmpt.Song.sequences -> [{openmpt.Song.Sequence}]
-- Returns the length of all detected subsongs. every entry has the following
-- properties: duration (in seconds), start_sequence, start_order, start_row
openmpt.Song.length -> [{table}]
openmpt.Song:focus()

-- Formats the given note index as a human-readable string.
-- If the instrument parameter is provided (either as an instrument index or as
-- an instrument object), and the corresponding instrument uses a custom tuning,
-- then the note names provided by that tuning are used.
-- If use_flats is omitted, note names as configured in the tracker (flats or
-- sharps) are used.
-- If use_flats is provided, note names are returned with flat accidentals if
-- true or sharp accidentals if false.
-- use_flats is ignored when a custom tuning is associated with the provided
-- instrument.
openmpt.Song:note_name(note [, instrument [, use_flats])

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
-- The channel's volume, ranging from 0 to 1.
openmpt.Song.Channel.volume -> [number]
-- The channel's panning position, ranging from -1.0 (left) to 1.0 (right).
openmpt.Song.Channel.panning -> [number]
-- The channel's surround status.
openmpt.Song.Channel.surround -> [boolean]
-- The channel's output plugin.
-- When reading this property, an openmpt.Song.Plugin is returned.
-- To set this property, either an openmpt.Song.Plugin object or a plugin slot
-- index (0 = no plugin, 1+ = plugin slots) or nil can be provided.
openmpt.Song.Channel.plugin -> [openmpt.Song.Plugin or number]

--------------------------------------------------------------------------------
-- openmpt.Song.Sample
--------------------------------------------------------------------------------

openmpt.Song.Sample
				//, "save"
				//, "load"
-- The index of the sample, e.g. 1 for the first sample.
openmpt.Song.Sample.id -> [read-only, number]
-- The sample name.
openmpt.Song.Sample.name -> [string]
-- The sample filename.
openmpt.Song.Sample.filename -> [string]
-- The on-disk sample path (only valid for external samples).
openmpt.Song.Sample.path -> [read-only, string]
-- True if this is an external sample.
openmpt.Song.Sample.external -> [read-only, boolean]
-- True if the external sample has been modified.
openmpt.Song.Sample.modified -> [read-only, boolean]
-- True if this sample slot contains an OPL instrument
openmpt.Song.Sample.opl -> [boolean]
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
-- The sampling frequency of a middle-C.
openmpt.Song.Sample.frequency -> [number]
-- The panning position, ranging from -1.0 (left) to 1.0 (right) or nil if no panning is enforced
openmpt.Song.Sample.panning -> [number or nil]
-- The default sample volume in range [0.0, 1.0]
openmpt.Song.Sample.volume -> [number]
-- The global volume in range [0.0, 1.0]
openmpt.Song.Sample.global_volume -> [number]
-- A list of cue points for this sample
openmpt.Song.Sample.cue_points -> [{number}]
-- Auto-vibrato type.
openmpt.Song.Sample.vibrato_type -> [openmpt.Song.vibratoType]
-- Auto-vibrato depth.
openmpt.Song.Sample.vibrato_depth -> [number]
-- Auto-vibrato sweep.
openmpt.Song.Sample.vibrato_sweep -> [number]
-- Auto-vibrato rate.
openmpt.Song.Sample.vibrato_rate -> [number]

-- Plays the given note of the sample, at a volume in [0.0, 1.0], at a panning position in [-1.0, 1.0]
-- Returns the channel on which the note is played on success, or nil otherwise 
openmpt.Song.Sample:play(note [, volume [, panning]])

-- Stops the note playing on the given channel (e.g. returned by play())
openmpt.Song.Sample:stop(channel)

-- TODO
openmpt.Song.Sample:get_data()
openmpt.Song.Sample:set_data()

-- Normalizes the sample data.
-- If start / end are specified, only this part of the sample is processed.
-- Returns true if any sample data was modified.
openmpt.Song.Sample:normalize([start [, end]]) -> [boolean]

-- Normalizes the sample data.
-- If start / end are specified, only this part of the sample is processed.
-- Returns the amount of DC offset that was removed, 0.0 if no change was made.
openmpt.Song.Sample:remove_dc_offset([start [, end]]) -> [number]

-- Amplifies the sample data.
-- If only amplify_start is provided, the whole sample is amplified by amplify_start.
-- If only amplify_start and amplify_end are provided, a smooth fade is applied from the sample start (amplified by amplify_start) to the sample end (amplified by amplify_end).
-- If start and end are provided as well, the fade is only applied to that portion of the sample.
-- Returns true on success.
openmpt.Song.Sample:amplify(amplify_start [, amplify_end [, start[, end]]) -> [boolean]

-- Changes the stereo separation of the sample data by a factor between 0 (make mono) and 2 (double the separation)
-- If start / end are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:change_stereo_separation(factor [, start[, end]]) -> [boolean]

-- Reverses the sample data.
-- If start / end are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:reverse([start[, end]]) -> [boolean]

-- Inverts the sample data.
-- If start / end are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:invert([start[, end]]) -> [boolean]

-- Applies a theoretical signed<->unsigned conversion to the sample data.
-- If start / end are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:unsign([start[, end]]) -> [boolean]

-- Silences part of the sample.
-- If start / end are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:silence([start[, end]]) -> [boolean]

-- Performs high-quality frequency domain pitch shifting and time stretching.
-- A stretch_factor of 1.0 does not alter the sample length, 2.0 makes it twice as long, etc.
-- A pitch_factor of 1.0 does not alter the sample length, 2.0 raises it by one octave, etc. Use math.pow(2, semitones / 12) to convert semitones to pitch factors.
-- If start / end are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:stretch_hq(stretch_factor, [pitch_factor, [start[, end]]]) -> [boolean]

-- Performs lower-quality time domain pitch shifting and time stretching.
-- A stretch_factor of 1.0 does not alter the sample length, 2.0 makes it twice as long, etc.
-- A pitch_factor of 1.0 does not alter the sample length, 2.0 raises it by one octave, etc. Use math.pow(2, semitones / 12) to convert semitones to pitch factors.
-- The grain_size determines the sound quality, with lower values providing a more metallic sound.
-- If start / end are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:stretch_lofi(stretch_factor, [pitch, [grain_size, [start[, end]]]) -> [boolean]

-- Resample the whole sample or part of the sample.
-- interpolation_type is any of openmpt.Song.interpolationType
-- If update_pattern_offsets is true (default false), the pattern offsets are adjusted accordingly (if possible).
-- If update_pattern_notes is true (default false), the pattern offsets are adjusted accordingly (if possible; MOD format only).
-- If start / end are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:resample(new_frequency, interpolation_type [, update_pattern_offsets [, update_pattern_notes [, start[, end]]]]) -> [boolean]

			//, "autotune"



--------------------------------------------------------------------------------
-- openmpt.Song.vibratoType
--------------------------------------------------------------------------------

openmpt.Song.vibratoType.SINE
openmpt.Song.vibratoType.SQUARE
openmpt.Song.vibratoType.RAMP_UP
openmpt.Song.vibratoType.RAMP_DOWN
openmpt.Song.vibratoType.RANDOM


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
-- openmpt.Song.interpolationType
--------------------------------------------------------------------------------

openmpt.Song.interpolationType.DEFAULT
openmpt.Song.interpolationType.NEAREST
openmpt.Song.interpolationType.LINEAR
openmpt.Song.interpolationType.CUBIC
openmpt.Song.interpolationType.SINC
openmpt.Song.interpolationType.R8BRAIN


--------------------------------------------------------------------------------
-- openmpt.Song.oplWaveform
--------------------------------------------------------------------------------

openmpt.Song.oplWaveform.SINE
openmpt.Song.oplWaveform.HALF_SINE
openmpt.Song.oplWaveform.ABSOLUTE_SINE
openmpt.Song.oplWaveform.PULSE_SINE
openmpt.Song.oplWaveform.SINE_EVEN_PERIODS
openmpt.Song.oplWaveform.ABSOLUTE_SINE_EVEN_PERIODS
openmpt.Song.oplWaveform.SQUARE
openmpt.Song.oplWaveform.DERIVED_SQUARE


--------------------------------------------------------------------------------
-- openmpt.Song.OPLData
--------------------------------------------------------------------------------

-- True if the two operators are played in parallel instead of modulating each other
openmpt.Song.OPLData.additive -> [boolean]
-- Amount of feedback to apply to the modulator, in range 0..7
openmpt.Song.OPLData.feedback -> [number]
openmpt.Song.OPLData.carrier -> [openmpt.Song.OPLOperator]
openmpt.Song.OPLData.modulator -> [openmpt.Song.OPLOperator]


--------------------------------------------------------------------------------
-- openmpt.Song.OPLOperator
--------------------------------------------------------------------------------

-- ADSR envelope, attack time, in range 0..15
openmpt.Song.OPLData.attack -> [number]
-- ADSR envelope, decay time, in range 0..15
openmpt.Song.OPLData.decay -> [number]
-- ADSR envelope, sustain level, in range 0..15
openmpt.Song.OPLData.sustain -> [number]
-- ADSR envelope, release time, in range 0..15
openmpt.Song.OPLData.release -> [number]

-- True if sustain level is held until note-off
openmpt.Song.OPLData.hold_sustain -> [boolean]

-- Operator volume in rage 0..63
openmpt.Song.OPLData.volume -> [number]

-- True if envelope duration scales with keys
openmpt.Song.OPLData.scale_envelope_with_keys -> [boolean]
-- Amount of scaling to apply to the envelope duration, in range 0..3 
openmpt.Song.OPLData.key_scale_level -> [number]

-- Frequency multiplier, in range 0..15
openmpt.Song.OPLData.frequency_multiplier -> [number]

-- The operator's waveform, one of openmpt.Song.oplWaveform
openmpt.Song.OPLData.waveform -> [openmpt.Song.oplWaveform]

-- True if vibrato is enabled for this operator.
openmpt.Song.OPLData.vibrato -> [boolean]
-- True if tremolo is enabled for this operator.
openmpt.Song.OPLData.tremolo -> [boolean]


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

openmpt.Song.Instrument.volume_envelope -> [openmpt.Song.Envelope] 
openmpt.Song.Instrument.panning_envelope -> [openmpt.Song.Envelope]
openmpt.Song.Instrument.pitch_envelope -> [openmpt.Song.Envelope]

openmpt.Song.Instrument.note_map -> [{openmpt.Song.NoteMapEntry}]

openmpt.Song.Instrument.tuning -> [string]

-- A list of all referenced samples
openmpt.Song.Instrument.samples -> [{openmpt.Song.Sample}]

-- Plays the given note of the instrument, at a volume in [0.0, 1.0], at a panning position in [-1.0, 1.0]
-- Returns the channel on which the note is played on success, or nil otherwise 
openmpt.Song.Instrument:play(note [, volume [, panning]])

-- Stops the note playing on the given channel (e.g. returned by play())
openmpt.Song.Instrument:stop(channel)

--------------------------------------------------------------------------------
-- openmpt.Song.Pattern
--------------------------------------------------------------------------------

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

--------------------------------------------------------------------------------
-- openmpt.Song.Sequence
--------------------------------------------------------------------------------

openmpt.Song.Sequence.id -> [read-only, number]
openmpt.Song.Sequence.name -> [string]
openmpt.Song.Sequence.initial_tempo -> [number]
openmpt.Song.Sequence.initial_speed -> [number]
openmpt.Song.Sequence.restart_position -> [number]
-- The pattern sequence.
openmpt.Song.Sequence.patterns -> [{number}]

--------------------------------------------------------------------------------
-- openmpt.Song.Plugin
--------------------------------------------------------------------------------

openmpt.Song.Plugin.name -> [string]
openmpt.Song.Plugin.library_name -> [read-only, string]
openmpt.Song.Plugin.is_loaded -> [read-only, boolean]
openmpt.Song.Plugin.index -> [read-only, number]
-- 0-based table of plugin parameters
openmpt.Song.Plugin.parameters -> [{openmpt.Song.PluginParameter}]
				//, "toggle_editor()"
-- True if this is a master plugin.
openmpt.Song.Plugin.master -> [boolean]
-- True if the dry signal is added back to the plugin mix.
openmpt.Song.Plugin.dry_mix -> [boolean]
-- True if the dry/wet curve is expanded.
openmpt.Song.Plugin.expanded_mix -> [boolean]
-- True if auto-suspension is enabled for this plugin.
openmpt.Song.Plugin.auto_suspend -> [boolean]
-- The bypass state of the plugin.
openmpt.Song.Plugin.bypass -> [boolean]
-- The dry/wet ratio, ranging from 0 (100% wet / 0% dry) to 1 (0% wet / 100% dry).
openmpt.Song.Plugin.dry_wet_ratio -> [number]
				//, "mixmode"
-- Plugin post-gain factor.
openmpt.Song.Plugin.gain -> [number]
-- The plugin to which the output of this plugin is routed.
-- When reading this property, an openmpt.Song.Plugin is returned.
-- To set this property, either an openmpt.Song.Plugin object or a plugin slot
-- index (0 = no plugin, 1+ = plugin slots) or nil can be provided.
openmpt.Song.Plugin.output -> [openmpt.Song.Plugin or number]

--------------------------------------------------------------------------------
-- openmpt.Song.PluginParameter
--------------------------------------------------------------------------------

-- The index of the plugin parameter, e.g. 0 for the first parameter.
openmpt.Song.PluginParameter.id -> [read-only, number]
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

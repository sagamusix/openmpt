--------------------------------------------------------------------------------
--  OpenMPT Scripting API
--  TODO: Convert to LuaCATS format
--
--  Naming conventions:
--  * Namespaces are lowercase
--  * Class names and enum names are CamelCase.
--  * Constants are ALL_CAPS
--
--  All strings are UTF-8.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- openmpt namespace
--------------------------------------------------------------------------------

-- Functions

-- Registers a callback function that will be called when specific OpenMPT
-- events occur.
-- Returns the callback ID which can be used to remove the callback.
openmpt.register_callback(type, function) -> [number]

-- Creates a new song. The song format (one of openmpt.Song.Format) can be
-- specified optionally. If it is not specified, the default settings are used.
openmpt.new_song([format]) -> [openmpt.Song object or nil]

-- Returns the last focussed song
openmpt.active_song() -> [openmpt.Song object or nil]

-- Returns the currently playing song
openmpt.playing_song() -> [openmpt.Song object or nil]

--------------------------------------------------------------------------------
-- openmpt.util namespace
--------------------------------------------------------------------------------

-- Functions

-- Announces text using the operating system's speech synthesizer.
-- Note that depending on the audio driver used by OpenMPT, speech may not be
-- audible if OpenMPT uses the audio device exclusively.
-- text: The text to speak.
-- rate: Optional speech rate modifier in range [-1.0, 1.0] where -1.0 is
--       slowest, 0.0 is normal, and 1.0 is fastest
openmpt.util.accessibility_announce(text [, rate]) -> nil

-- Returns the contents of a Lua table as a string.
openmpt.util.dump(object) -> [string]

-- Evaluates the expression string as Lua code and returns the result.
-- Example: openmpt.util.eval("return 3 + 4") returns 7.
openmpt.util.eval(expression) -> [object]

-- Converts a linear volume level to decibel (0 = -inf dB, 1 = 0dB)
openmpt.util.linear_to_db(linear) -> [number]

-- Converts a volume level in decibel to linear (-inf dB = 0, 0 dB = 1)
openmpt.util.db_to_linear(decibel) -> [number]

-- Translates a cutoff value in [0, 1.0] to frequency in Hz for the given song.
openmpt.util.cutoff_to_frequency(song, cutoff) -> [number]

-- Translates a frequency value to cutoff value in [0, 1.0] for the given song.
openmpt.util.frequency_to_cutoff(song, frequency) -> [number]

-- Converts MOD/XM transpose and finetune values to frequency in Hz.
-- transpose: Transpose value in semitones (-128 to 127)
-- finetune: Finetune value in 1/128th semitones (-128 to 127)
--           To convert MOD finetune values, simply multiply them by 16.
openmpt.util.transpose_to_frequency(transpose, finetune) -> [number]

-- Converts frequency in Hz to MOD/XM transpose and finetune values.
-- Returns a table with 'transpose' and 'finetune' fields.
-- transpose: Transpose value in semitones (-128 to 127)
-- finetune: Finetune value in 1/128th semitones (-128 to 127)
--           To obtain MOD finetune values, simply divide this value by 16.
openmpt.util.frequency_to_transpose(frequency) -> [table]

-- Computes FFT of provided real and optional imaginary signals.
-- The input table length must be a power of 2.
-- The result is in bit-reversed order.
openmpt.util.fft(real [, imaginary])

-- Computes IFFT of provided real and optional imaginary signals.
-- The input table length must be a power of 2.
-- The input is expected to be in bit-reversed order.
openmpt.util.ifft(real [, imaginary])

--------------------------------------------------------------------------------
-- openmpt.app namespace
--------------------------------------------------------------------------------

-- Constants

-- The Scripting API version of this OpenMPT instance, currently 1.0
-- The API version follows semantic versioning (https://semver.org/).
-- Practically, this means that minor increments (e.g. 1.0 -> 1.1) may add new
-- functionality to the API. Changes that are not backwards-compatible, such as
-- removal or renaming of an API element are only done in major increments
-- (e.g. 1.9 -> 2.0)
openmpt.app.API_VERSION -> [read-only, number]

-- The OpenMPT version
-- Example: "1.33.01.00"
openmpt.app.OPENMPT_VERSION -> [read-only, string]

-- Properties

-- Enable or disable the metronome.
openmpt.app.metronome_enabled -> [boolean]

-- The metronome volume in decibels, ranging from -48dB to 0dB.
openmpt.app.metronome_volume -> [number]

-- Functions

-- Opens the specified song file(s)
-- Returns the opened songs.
-- Example: openmpt.app.load("c:/module1.it", "c:/module2.s3m")
openmpt.app.load(filename1 [, filename2...]) -> [{openmpt.Song object}]

-- Returns a table of currently open songs.
openmpt.app.open_songs() -> [{openmpt.Song object}]

-- Returns a table of recently opened files.
openmpt.app.recent_songs() -> [{string}]

-- Returns a table of template song file paths.
openmpt.app.template_songs() -> [{string}]

-- Returns a table of example song file paths.
openmpt.app.example_songs() -> [{string}]

-- Like print(), but shows a message box.
openmpt.app.show_message(obj1 [, obj2...])

-- Shows a folder browser dialog and returns the selected folder path.
-- title: Optional title to show in the title bar of the folder browser dialog.
-- initial_path: Optional path to which the dialog navigates when it opens.
-- Returns an empty string if the dialog was cancelled.
openmpt.app.browse_for_folder([title [, initial_path]]) -> [string]

-- Shows a file open dialog and returns a table of selected file paths.
-- Returns an empty table if the dialog was cancelled.
-- title: Optional title to show in the title bar of the file browser dialog.
-- filters: An optional table of file format filters. Each entry should contain
--          a table with the following properties:
--          - filter: The file pattern (e.g. "*.lua")
--          - description: Optional description for the filter (defaults to the
--            filter pattern if missing)
--          Example: {{filter = "*.lua", description = "Lua Scripts"},
--                    {filter = "*.txt", description = "Text Files"}}
-- initial_path: Optional path to which the dialog navigates when it opens.
-- allow_multiple: If true, multiple files can be selected.
openmpt.app.file_load_dialog([title [, filters [, initial_path [, allow_multiple]]]]) -> [{string}]

-- Shows a file save dialog and returns the selected file path.
-- Returns an empty string if the dialog was cancelled.
-- title: Optional title to show in the title bar of the file browser dialog.
-- filters: An optional table of file format filters. Each entry should contain
--          a table with the following properties:
--          - filter: The file pattern (e.g. "*.lua")
--          - description: Optional description for the filter (defaults to the
--            filter pattern if missing)
--          Example: {{filter = "*.lua", description = "Lua Scripts"},
--                    {filter = "*.txt", description = "Text Files"}}
-- initial_path: Optional path to which the dialog navigates when it opens.
openmpt.app.file_save_dialog([title [, filters [, initial_path]]]) -> [string]

-- Registers keyboard shortcut to execute function func.
-- Returns the shortcut ID which is passed as a parameter to func and can be
-- used to remove the shortcut using openmpt.app.remove_shortcut.
-- Modifier keys are: SHIFT, CTRL, ALT, WIN, CC, RSHIFT, RCTRL, RALT
-- Regular keys are: A..Z, 0..9, NUM0..NUM9, F1..F24, CLEAR, RETURN, MENU,
-- PAUSE, CAPITAL, KANA, HANGUL, JUNJA, FINAL, HANJA, KANJI, ESC, CONVERT,
-- NONCONVERT, ACCEPT, MODECHANGE, " ", PREV, NEXT, END, HOME, LEFT, UP, RIGHT,
-- DOWN, SELECT, PRINT, EXECUTE, SNAPSHOT, INSERT, DELETE, HELP, APPS, SLEEP,
-- NUM*, NUM+, NUM-, NUM., NUM/
-- The callback function also receives the shortcut ID as a parameter.
openmpt.app.register_shortcut(func, key...) -> [number]

-- Remove a shortcut previously registered using openmpt.app.register_shortcut.
-- Return true if the shortcut ID was found and removed.
openmpt.app.remove_shortcut(id) -> [boolean]

-- Returns a list of all known plugins. Every entry has the following properties:
-- library_name: The plugin name
-- path: Path to the plugin file, or other identifier
-- id: The unique ID that identifies the plugin together with its library_name.
-- is_instrument: True if this is an instrument plugin
-- is_builtin: True if the plugin is shipped with OpenMPT
-- vendor: The plugin vendor
-- tags: A string of user-supplied tags
openmpt.app.registered_plugins() -> [{table}]

-- Registers a MIDI filter function that is applied to incoming MIDI messages.
-- Returns the callback ID which is passed as a parameter to func and can be
-- used to remove the filter using openmpt.app.remove_midi_filter.
-- The function receives an array of bytes and is expected to return a message
-- in the same format. Returning an empty array drops the message, any other
-- value modifies the received message.
-- The second parameter to the function identifies the device from which the
-- message was received, the third parameter is the callback ID.
openmpt.app.register_midi_filter(func) -> [number]

-- Remove a filter previously registered using openmpt.app.register_midi_filter.
-- Return true if the callback ID was found and removed.
openmpt.app.remove_midi_filter(id) -> [boolean]

-- Retrieves value for the given key within the given section.
-- If the key is not found in the section, the optional default value is
-- returned.
openmpt.app.get_state(section, key [, default_value]) -> [string]

-- Set the value for the given key within the given section.
-- If the optional parameter persist is set to true, the value will be
-- remembered even after restarting OpenMPT.
openmpt.app.set_state(section, key, value [, persist])

-- Deletes the value for the given key within the given section.
openmpt.app.delete_state(section, key) -> nil

-- Checks if the specified permission(s) have been granted to the script.
-- permission: Can be either a single permission string or a table of permission
--             strings. When a table is provided, returns true only if ALL
--             permissions are granted.
-- Returns true if the permission(s) are granted, false otherwise.
openmpt.app.check_permission(permission) -> [boolean]

-- Requests permission(s) from the user if not already granted.
-- permission: Can be either a single permission string or a table of permission
--             strings.
-- This function has an "all-or-nothing" approach: Either all permissions are
-- granted, or none (unless they were previously granted). As such, if at least
-- one permission was previously denied, the other permissions will not be
-- granted as well.
-- Returns true if all requested permissions were granted (either previously or
-- by user), false otherwise.
openmpt.app.request_permission(permission) -> [boolean]

-- Sets a timer that calls the provided function after the specified duration
-- has passed.
-- func: The function to call when the timer expires.
-- duration: The duration in seconds before the function is called.
-- Returns the timer ID which is passed as a parameter to func and can be
-- used to remove the shortcut using openmpt.app.cancel_timer.
-- If the timer could not be created, nil is returned.
-- The callback function also receives the timer ID as a parameter.
openmpt.app.set_timer(func, duration) -> [number or nil]

-- Cancels a timer previously created with openmpt.app.set_timer.
-- id: The timer ID returned by openmpt.app.set_timer.
-- Return true if the timer ID was found and removed.
openmpt.app.cancel_timer(id) -> nil

-- Shows a customizable task dialog.
-- config: A table containing dialog configuration with the following optional
--         properties:
--         - title: Dialog title string
--         - description: Main dialog description text
--         - choices: Table of (radio) buttons display, with the following
--                    properties:
--                    - text: The button text
--                    - type: "button" (default) or "radio"
--                    - enabled: true (default) or false
--                    - id: Numeric ID for the choice
--         - buttons: Table specifying standard buttons to include:
--                    "ok", "yes", "no", "cancel", "retry", "close"
--         - footer: Footer text string
--         - expansion: Table for expandable section:
--                      - info: The expanded text content
--                      - collapsed_label: Label when collapsed
--                      - expanded_label: Label when expanded
--                      - expanded: Set to true if the section should be
--                        expanded by default
--         - verification: Verification checkbox text
--         - default_choice: Default choice button ID
--         - default_radio: Default radio button ID
-- Returns a table with the following properties:
-- - button: The ID of the clicked button (or standard button constant)
-- - radio: The ID of the selected radio button
-- - checked: Boolean indicating if verification checkbox was checked.
openmpt.app.task_dialog(config) -> [table]

--------------------------------------------------------------------------------
-- openmpt.Song
--------------------------------------------------------------------------------

-- Properties

-- True if the song has been modified since the last save.
openmpt.Song.modified -> [read-only, boolean]
-- The on-disk path of the file, may be empty if the file has not been saved yet.
openmpt.Song.path -> [read-only, string]
-- The song name.
openmpt.Song.name -> [string]
-- The song artist.
openmpt.Song.artist -> [string]
-- The song format (MOD, XM, S3M, IT, MPTM).
openmpt.Song.type -> [openmpt.Song.Format]
-- An array containing the information about all channels in the song.
openmpt.Song.channels -> [{openmpt.Song.Channel}]
-- The master sample volume multiplier in [0, 2000]
openmpt.Song.sample_volume -> [number]
-- The master plugin volume multiplier in [0, 2000]
openmpt.Song.plugin_volume -> [number]
-- The initial global volume of the song in [0, 1.0]
openmpt.Song.initial_global_volume -> [number]
-- The current global volume of the song in [0, 1.0]
openmpt.Song.current_global_volume -> [number]
-- The current tempo of the song. Use openmpt.Song:approximate_real_bpm() to calculate the approximate BPM instead.
openmpt.Song.current_tempo -> [number]
-- The current speed (ticks per row) of the song.
openmpt.Song.current_speed -> [number]
-- The time signature of the song. Can be overridden by pattern-specific time signatures.
openmpt.Song.time_signature -> [openmpt.Song.TimeSignature]
-- An array containing all samples in the song.
openmpt.Song.samples -> [{openmpt.Song.Sample}]
-- An array containing all instruments in the song.
openmpt.Song.instruments -> [{openmpt.Song.Instrument}]
-- An array containing all patterns in the song. Note that the first pattern has index 0!
openmpt.Song.patterns -> [{openmpt.Song.Pattern}]
-- An array containing all plugin slots in the song.
openmpt.Song.plugins -> [{openmpt.Song.Plugin}]
-- An array containing all sequences in the song.
openmpt.Song.sequences -> [{openmpt.Song.Sequence}]
-- An array containing all custom tunings defined in the song.
openmpt.Song.tunings -> [{openmpt.Song.Tuning}]
-- Global song flag: Use linear frequency slides instead of Amiga-style slides.
openmpt.Song.linear_frequency_slides -> [boolean]
-- Global song flag: Use old Impulse Tracker effect implementations.
openmpt.Song.it_old_effects -> [boolean]
-- Global song flag: Execute volume slides on every tick (Scream Tracker 3.0 style).
openmpt.Song.fast_volume_slides -> [boolean]
-- Global song flag: Enforce Amiga frequency limits.
openmpt.Song.amiga_frequency_limits -> [boolean]
-- Global song flag: Enable extended filter range.
openmpt.Song.extended_filter_range -> [boolean]
-- Global song flag: Use ProTracker 1/2 playback mode.
openmpt.Song.protracker_mode -> [boolean]
-- Mix levels for volume level compatibility with older OpenMPT versions and other software.
openmpt.Song.MixLevels -> [openmpt.Song.MixLevels]
-- Tempo mode determining how effective tempo is calculated.
openmpt.Song.TempoMode -> [openmpt.Song.TempoMode]
-- The length of all detected subsongs. Every entry has the following
-- properties: duration (in seconds), start_sequence, start_order, start_row
openmpt.Song.length -> [{table}]
-- Playback lock. Can be nil (no lock) or a tuple {start_order, end_order} specifying the locked order range.
openmpt.Song.playback_range_lock -> [nil or {number, number}]
-- The duration for how long the song has been playing, in seconds.
openmpt.Song.elapsed_time -> [number]
-- True if the song is the one that is currently playing.
openmpt.Song.is_playing -> [boolean]
-- True if a window belonging to the song is currently focussed.
openmpt.Song.is_active -> [boolean]

-- Functions

-- Saves the song. If no filename is provided, the current filename is used.
openmpt.Song:save([filename]) -> nil

-- Closes the song. Note that this call may fail if there are any open dialogs.
-- force: If true, no save prompt is shown if the module was modified.
openmpt.Song:close([force]) -> nil

-- Calculates the approximate real BPM based on the current tempo, speed, time signature and tempo mode settings.
openmpt.Song:approximate_real_bpm() -> [number]

-- Sets focus to a window belonging to this song.
openmpt.Song:focus() -> nil

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
openmpt.Song:note_name(note [, instrument [, use_flats]]) -> [string]

-- Creates a new view for this song. Return true if a new window was created.
openmpt.Song:new_window() -> [boolean]

-- Sets the playback position to the specified time in seconds.
openmpt.Song:set_playback_position_seconds(seconds) -> nil

-- Sets the playback position to the specified order and row.
openmpt.Song:set_playback_position(order, row) -> nil

-- Gets the playback time at the specified order and row position.
openmpt.Song:get_playback_time_at(order, row) -> [number]

--------------------------------------------------------------------------------
-- openmpt.Song.TimeSignature
--------------------------------------------------------------------------------

-- Properties

-- The number of rows that make up one beat.
openmpt.Song.TimeSignature.rows_per_beat -> [number]
-- The number of rows that make up one measure.
openmpt.Song.TimeSignature.rows_per_measure -> [number]
-- A table containing swing timing values, with as many entries as there are
-- rows per beat, or nil if swing is not used.
openmpt.Song.TimeSignature.swing -> [{number}]

--------------------------------------------------------------------------------
-- openmpt.Song.Format enum
--------------------------------------------------------------------------------

openmpt.Song.Format.MOD = "MOD"
openmpt.Song.Format.XM = "XM"
openmpt.Song.Format.S3M = "S3M"
openmpt.Song.Format.IT = "IT"
openmpt.Song.Format.MPTM = "MPTM"

--------------------------------------------------------------------------------
-- openmpt.Song.MixLevels enum
--------------------------------------------------------------------------------

openmpt.Song.MixLevels.ORIGINAL = "ORIGINAL"
openmpt.Song.MixLevels.V1_17RC1 = "V1_17RC1"
openmpt.Song.MixLevels.V1_17RC2 = "V1_17RC2"
openmpt.Song.MixLevels.V1_17RC3 = "V1_17RC3"
openmpt.Song.MixLevels.COMPATIBLE = "COMPATIBLE"
openmpt.Song.MixLevels.COMPATIBLE_FT2 = "COMPATIBLE_FT2"

--------------------------------------------------------------------------------
-- openmpt.Song.TempoMode enum
--------------------------------------------------------------------------------

openmpt.Song.TempoMode.CLASSIC = "CLASSIC"
openmpt.Song.TempoMode.ALTERNATIVE = "ALTERNATIVE"
openmpt.Song.TempoMode.MODERN = "MODERN"

--------------------------------------------------------------------------------
-- openmpt.Song.Transpose enum
--------------------------------------------------------------------------------

openmpt.Song.Transpose.OCTAVE_DOWN = "OCTAVE_DOWN"
openmpt.Song.Transpose.OCTAVE_UP = "OCTAVE_UP"

--------------------------------------------------------------------------------
-- openmpt.Song.Channel
--------------------------------------------------------------------------------

-- Properties

-- The index of the channel, e.g. 1 for the first channel.
openmpt.Song.Channel.index -> [read-only, number]
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

-- Functions

-- Solos this channel by muting all other channels in the song.
openmpt.Song.Channel:solo() -> nil

--------------------------------------------------------------------------------
-- openmpt.Song.ChannelList
--------------------------------------------------------------------------------

-- Functions

-- Rearranges channels in the song according to the provided order.
-- Returns true if the rearrangement was successful, false otherwise.
-- new_order: Table containing channel indices (1-based) or nil values
--            - Channel indices (1, 2, 3, etc.) refer to existing channels
--            - nil values create new empty channels at this position
--
-- Example: song.channels:rearrange_channels({2, 1, nil, 3})
-- This moves channel 2 to position 1, channel 1 to position 2, creates a new
-- empty channel at position 3, and moves channel 3 to position 4.
-- If there were any more channels in the module, they are discarded.
openmpt.Song.ChannelList:rearrange(new_order) -> [boolean]

--------------------------------------------------------------------------------
-- openmpt.Song.Sample
--------------------------------------------------------------------------------

-- Properties

-- The index of the sample, e.g. 1 for the first sample.
openmpt.Song.Sample.index -> [read-only, number]
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
openmpt.Song.Sample.vibrato_type -> [openmpt.Song.VibratoType]
-- Auto-vibrato depth.
openmpt.Song.Sample.vibrato_depth -> [number]
-- Auto-vibrato sweep.
openmpt.Song.Sample.vibrato_sweep -> [number]
-- Auto-vibrato rate.
openmpt.Song.Sample.vibrato_rate -> [number]

-- Functions

-- TODO save

-- Loads sample data from a file.
-- Returns true on success.
openmpt.Song.Sample:load(filename) -> [boolean]

-- Plays the given note of the sample, at a volume in [0.0, 1.0], at a panning
-- position in [-1.0, 1.0].
-- Returns the channel on which the note is played on success, or nil otherwise.
openmpt.Song.Sample:play(note [, volume [, panning]]) -> [number or nil]

-- Stops the note playing on the given channel (e.g. returned by play())
openmpt.Song.Sample:stop(channel) -> nil

-- Returns the sample data as a table of floating-point values.
-- For stereo samples, the values are interleaved (left, right, left, right, ...).
-- The values are normalized to the range [-1.0, 1.0].
-- Returns an empty table if the sample has no data.
-- Raises error if the sample is an OPL instrument.
openmpt.Song.Sample:get_data() -> [{number}]

-- Sets the sample data from a table of floating-point values.
-- For stereo samples, the values must be interleaved (left, right, left, right, ...).
-- The values should be in the range [-1.0, 1.0] and will be scaled
-- appropriately for the sample's bit depth.
-- You can pass an empty table to free the sample data.
-- Raises error if the sample is an OPL instrument.
openmpt.Song.Sample:set_data(data) -> nil

-- Normalizes the sample data.
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns true if any sample data was modified.
openmpt.Song.Sample:normalize([start_point [, end_point]]) -> [boolean]

-- Normalizes the sample data.
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns the amount of DC offset that was removed, 0.0 if no change was made.
openmpt.Song.Sample:remove_dc_offset([start_point [, end_point]]) -> [number]

-- Amplifies the sample data.
-- If only amplify_start is provided, the whole sample is amplified by amplify_start.
-- If only amplify_start and amplify_end are provided, a smooth fade is applied from the sample start (amplified by amplify_start) to the sample end (amplified by amplify_end).
-- If start_point and end_point are provided as well, the fade is only applied to that portion of the sample.
-- Returns true on success.
openmpt.Song.Sample:amplify(amplify_start [, amplify_end [, start_point[, end_point]]) -> [boolean]

-- Changes the stereo separation of the sample data by a factor between 0 (make mono) and 2 (double the separation)
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:change_stereo_separation(factor [, start_point[, end_point]]) -> [boolean]

-- Reverses the sample data.
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:reverse([start_point [, end_point]]) -> [boolean]

-- Inverts the sample data.
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:invert([start_point [, end_point]]) -> [boolean]

-- Applies a theoretical signed<->unsigned conversion to the sample data.
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:unsign([start_point [, end_point]]) -> [boolean]

-- Silences part of the sample.
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:silence([start_point [, end_point]]) -> [boolean]

-- Performs high-quality frequency domain pitch shifting and time stretching.
-- A stretch_factor of 1.0 does not alter the sample length, 2.0 makes it twice as long, etc.
-- A pitch_factor of 1.0 does not alter the sample length, 2.0 raises it by one octave, etc. Use math.pow(2, semitones / 12) to convert semitones to pitch factors.
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:stretch_hq(stretch_factor [, pitch_factor [, start_point [, end_point]]]) -> [boolean]

-- Performs lower-quality time domain pitch shifting and time stretching.
-- A stretch_factor of 1.0 does not alter the sample length, 2.0 makes it twice as long, etc.
-- A pitch_factor of 1.0 does not alter the sample length, 2.0 raises it by one octave, etc. Use math.pow(2, semitones / 12) to convert semitones to pitch factors.
-- The grain_size determines the sound quality, with lower values providing a more metallic sound.
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:stretch_lofi(stretch_factor [,pitch [,grain_size [, start_point [, end_point]]]]) -> [boolean]

-- Resample the whole sample or part of the sample.
-- interpolation_type is any of openmpt.Song.InterpolationType
-- If update_pattern_offsets is true (default false), the pattern offsets are adjusted accordingly (if possible).
-- If update_pattern_notes is true (default false), the pattern offsets are adjusted accordingly (if possible; MOD format only).
-- If start_point / end_point are specified, only this part of the sample is processed.
-- Returns true on success.
openmpt.Song.Sample:resample(new_frequency, interpolation_type [, update_pattern_offsets [, update_pattern_notes [, start_point [, end_point]]]]) -> [boolean]

-- openmpt.Song.Sample:autotune not implemented

--------------------------------------------------------------------------------
-- openmpt.Song.VibratoType enum
--------------------------------------------------------------------------------

openmpt.Song.VibratoType.SINE = "SINE"
openmpt.Song.VibratoType.SQUARE = "SQUARE"
openmpt.Song.VibratoType.RAMP_UP = "RAMP_UP"
openmpt.Song.VibratoType.RAMP_DOWN = "RAMP_DOWN"
openmpt.Song.VibratoType.RANDOM = "RANDOM"

--------------------------------------------------------------------------------
-- openmpt.Song.LoopType enum
--------------------------------------------------------------------------------

openmpt.Song.LoopType.OFF = "OFF"
openmpt.Song.LoopType.FORWARD = "FORWARD"
openmpt.Song.LoopType.PINGPONG = "PINGPONG"

--------------------------------------------------------------------------------
-- openmpt.Song.SampleLoop
--------------------------------------------------------------------------------

-- Properties

-- The loop start and end point. The end point is exclusive.
openmpt.Song.SampleLoop.range -> [array of two numbers]
-- The loop type (off, forward, ping-pong)
openmpt.Song.SampleLoop.type -> [openmpt.Song.LoopType]

-- Functions

-- Crossfades the loop for the given duration in seconds, using the specified
-- fade law (a value in the range [0.0 = constant volume, 1.0 = constant power]),
-- optionally fading back to the original sample data after the loop end (true by default).
-- Returns true on success.
openmpt.Song.SampleLoop:crossfade(duration[, fade_law[, fade_after_loop]]) -> [boolean]

--------------------------------------------------------------------------------
-- openmpt.Song.InterpolationType enum
--------------------------------------------------------------------------------

openmpt.Song.InterpolationType.DEFAULT = "DEFAULT"
openmpt.Song.InterpolationType.NEAREST = "NEAREST"
openmpt.Song.InterpolationType.LINEAR = "LINEAR"
openmpt.Song.InterpolationType.CUBIC_SPLINE = "CUBIC_SPLINE"
openmpt.Song.InterpolationType.SINC = "SINC"
openmpt.Song.InterpolationType.R8BRAIN = "R8BRAIN"

--------------------------------------------------------------------------------
-- openmpt.Song.OPLWaveform enum
--------------------------------------------------------------------------------

openmpt.Song.OPLWaveform.SINE = "SINE"
openmpt.Song.OPLWaveform.HALF_SINE = "HALF_SINE"
openmpt.Song.OPLWaveform.ABSOLUTE_SINE = "ABSOLUTE_SINE"
openmpt.Song.OPLWaveform.PULSE_SINE = "PULSE_SINE"
openmpt.Song.OPLWaveform.SINE_EVEN_PERIODS = "SINE_EVEN_PERIODS"
openmpt.Song.OPLWaveform.ABSOLUTE_SINE_EVEN_PERIODS = "ABSOLUTE_SINE_EVEN_PERIODS"
openmpt.Song.OPLWaveform.SQUARE = "SQUARE"
openmpt.Song.OPLWaveform.DERIVED_SQUARE = "DERIVED_SQUARE"

--------------------------------------------------------------------------------
-- openmpt.Song.OPLData
--------------------------------------------------------------------------------

-- Properties

-- True if the two operators are played in parallel instead of modulating each other
openmpt.Song.OPLData.additive -> [boolean]
-- Amount of feedback to apply to the modulator, in range 0..7
openmpt.Song.OPLData.feedback -> [number]
openmpt.Song.OPLData.carrier -> [openmpt.Song.OPLOperator]
openmpt.Song.OPLData.modulator -> [openmpt.Song.OPLOperator]

--------------------------------------------------------------------------------
-- openmpt.Song.OPLOperator
--------------------------------------------------------------------------------

-- Properties

-- ADSR envelope, attack time, in range 0..15
openmpt.Song.OPLOperator.attack -> [number]
-- ADSR envelope, decay time, in range 0..15
openmpt.Song.OPLOperator.decay -> [number]
-- ADSR envelope, sustain level, in range 0..15
openmpt.Song.OPLOperator.sustain -> [number]
-- ADSR envelope, release time, in range 0..15
openmpt.Song.OPLOperator.release -> [number]

-- True if sustain level is held until note-off
openmpt.Song.OPLOperator.hold_sustain -> [boolean]

-- Operator volume in range 0..63
openmpt.Song.OPLOperator.volume -> [number]

-- True if envelope duration scales with keys
openmpt.Song.OPLOperator.scale_envelope_with_keys -> [boolean]
-- Amount of scaling to apply to the envelope duration, in range 0..3
openmpt.Song.OPLOperator.key_scale_level -> [number]

-- Frequency multiplier, in range 0..15
openmpt.Song.OPLOperator.frequency_multiplier -> [number]

-- The operator's waveform, one of openmpt.Song.OPLWaveform
openmpt.Song.OPLOperator.waveform -> [openmpt.Song.OPLWaveform]

-- True if vibrato is enabled for this operator.
openmpt.Song.OPLOperator.vibrato -> [boolean]
-- True if tremolo is enabled for this operator.
openmpt.Song.OPLOperator.tremolo -> [boolean]

--------------------------------------------------------------------------------
-- openmpt.Song.Instrument
--------------------------------------------------------------------------------

-- Properties

-- The index of the instrument, e.g. 1 for the first instrument.
openmpt.Song.Instrument.index -> [read-only, number]
-- The instrument name.
openmpt.Song.Instrument.name -> [string]
-- The instrument filename.
openmpt.Song.Instrument.filename -> [string]
-- The panning position, ranging from -1.0 (left) to 1.0 (right) or nil if no panning is enforced
openmpt.Song.Instrument.panning -> [number or nil]
-- The global volume in range [0.0, 1.0]
openmpt.Song.Instrument.global_volume -> [number]

-- The volume envelope of the instrument.
openmpt.Song.Instrument.volume_envelope -> [openmpt.Song.Envelope]
-- The panning envelope of the instrument.
openmpt.Song.Instrument.panning_envelope -> [openmpt.Song.Envelope]
-- The pitch/filter envelope of the instrument.
openmpt.Song.Instrument.pitch_envelope -> [openmpt.Song.Envelope]

-- The note-to-sample mapping table for the instrument.
openmpt.Song.Instrument.note_map -> [{openmpt.Song.NoteMapEntry}]

-- The name of the custom tuning associated with this instrument, or empty string if using standard tuning.
openmpt.Song.Instrument.tuning -> [string]

-- A list of all referenced samples
openmpt.Song.Instrument.samples -> [{openmpt.Song.Sample}]

-- Functions

-- TODO save

-- Loads instrument data from a file. Supports various instrument formats (XI, SFZ, etc.)
-- Returns true on success, false on failure.
openmpt.Song.Instrument:load(filename) -> [boolean]

-- Plays the given note of the instrument, at a volume in [0.0, 1.0], at a
-- panning position in [-1.0, 1.0].
-- Returns the channel on which the note is played on success, or nil otherwise.
openmpt.Song.Instrument:play(note [, volume [, panning]]) -> [number or nil]

-- Stops the note playing on the given channel (e.g. returned by play())
openmpt.Song.Instrument:stop(channel) -> nil

--------------------------------------------------------------------------------
-- openmpt.Song.Envelope
--------------------------------------------------------------------------------

-- Properties

-- Enable or disable the envelope.
openmpt.Song.Envelope.enabled -> [boolean]
-- Enable or disable the envelope carry feature.
openmpt.Song.Envelope.carry -> [boolean]
-- A table containing the envelope points
openmpt.Song.Envelope.points -> [{openmpt.Song.EnvelopeNode}]
-- True if the envelope loop is enabled.
openmpt.Song.Envelope.loop_enabled -> [boolean]
-- The index of the loop start point
openmpt.Song.Envelope.loop_start -> [number]
-- The index of the loop end point
openmpt.Song.Envelope.loop_end -> [number]
-- True if the envelope sustain loop is enabled.
openmpt.Song.Envelope.sustain_loop_enabled -> [boolean]
-- The index of the sustain loop start point
openmpt.Song.Envelope.sustain_loop_start -> [number]
-- The index of the sustain end start point
openmpt.Song.Envelope.sustain_loop_end -> [number]
-- The index of the release node, or nil if no release node is set.
openmpt.Song.Envelope.release_node -> [number or nil]

--------------------------------------------------------------------------------
-- openmpt.Song.EnvelopeNode
--------------------------------------------------------------------------------

-- Properties

-- The tick position of this envelope node.
openmpt.Song.EnvelopeNode.tick -> [number]
-- The value of this envelope node in [0, 1.0] for the volume and pitch/filter
-- envelopes, or in [-1.0, 1.0] for the pan envelope.
openmpt.Song.EnvelopeNode.value -> [number]

--------------------------------------------------------------------------------
-- openmpt.Song.NoteMapEntry
--------------------------------------------------------------------------------

-- Properties

-- The sample index assigned to this note (0 = no sample).
openmpt.Song.NoteMapEntry.sample -> [number]
-- The note transpose value in semitones.
openmpt.Song.NoteMapEntry.transpose -> [number]

--------------------------------------------------------------------------------
-- openmpt.Song.Pattern
--------------------------------------------------------------------------------

-- Properties

-- The index of the pattern e.g. 0 for the first pattern.
openmpt.Song.Pattern.index -> [read-only, number]
-- The pattern name.
openmpt.Song.Pattern.name -> [string]
-- The number of rows in the pattern.
openmpt.Song.Pattern.rows -> [number]
-- A two-dimensional array containing pattern data. Access with data[row][channel].
-- Note that rows are 0-based, while channel indices are 1-based (like in the
-- tracker interface).
openmpt.Song.Pattern.data -> [{openmpt.Song.PatternCell}]

-- The time signature of the pattern, or nil if the pattern does not override
-- the global time signature.
openmpt.Song.Pattern.time_signature -> [openmpt.Song.TimeSignature or nil]

-- Functions

-- Transpose all notes in the pattern by the given integer amount, which can
-- also be one of the special values openmpt.Song.Transpose.OCTAVE_DOWN and
-- openmpt.Song.Transpose.OCTAVE_UP
openmpt.Song.Pattern:transpose(steps) -> nil

--------------------------------------------------------------------------------
-- openmpt.Song.PatternCell
--------------------------------------------------------------------------------

-- Properties

-- The note value. (TODO: document special notes)
openmpt.Song.PatternCell.note -> [number]
-- The instrument or sample number.
openmpt.Song.PatternCell.instrument -> [number]
-- The volume command type.
openmpt.Song.PatternCell.volume_effect -> [number]
-- The volume command value.
openmpt.Song.PatternCell.volume_param -> [number]
-- The effect command.
openmpt.Song.PatternCell.effect -> [number]
-- The effect parameter.
openmpt.Song.PatternCell.param -> [number]

--------------------------------------------------------------------------------
-- openmpt.Song.Sequence
--------------------------------------------------------------------------------

-- Properties

-- The index of the sequence, e.g. 1 for the first sequence.
openmpt.Song.Sequence.index -> [read-only, number]
-- The sequence name.
openmpt.Song.Sequence.name -> [string]
-- The initial tempo for this sequence.
openmpt.Song.Sequence.initial_tempo -> [number]
-- The initial speed for this sequence.
openmpt.Song.Sequence.initial_speed -> [number]
-- The restart position for this sequence.
openmpt.Song.Sequence.restart_position -> [number]
-- The pattern sequence.
openmpt.Song.Sequence.patterns -> [{number}]

--------------------------------------------------------------------------------
-- openmpt.Song.Tuning
--------------------------------------------------------------------------------

-- Properties

-- The tuning name.
openmpt.Song.Tuning.name -> [string]
-- The tuning type.
openmpt.Song.Tuning.type -> [openmpt.Song.TuningType]
-- TODO group sizes etc.
-- The number of fine tuning steps between notes.
openmpt.Song.Tuning.fine_step_count -> [{number}]
-- TODO

--------------------------------------------------------------------------------
-- openmpt.Song.TuningType enum
--------------------------------------------------------------------------------

openmpt.Song.TuningType.GENERAL = "GENERAL"
openmpt.Song.TuningType.GROUPGEOMETRIC = "GROUPGEOMETRIC"
openmpt.Song.TuningType.GEOMETRIC = "GEOMETRIC"

--------------------------------------------------------------------------------
-- openmpt.Song.Plugin
--------------------------------------------------------------------------------

-- Properties

-- The index of the plugin slot, e.g. 1 for the first plugin slot.
openmpt.Song.Plugin.index -> [read-only, number]
-- The user-chosen display name of the plugin
openmpt.Song.Plugin.name -> [string]
-- The library name that identifies the plugin
openmpt.Song.Plugin.library_name -> [read-only, string]
-- True if there is a plugin loaded into this slot
openmpt.Song.Plugin.is_loaded -> [read-only, boolean]
-- 0-based table of plugin parameters
openmpt.Song.Plugin.parameters -> [{openmpt.Song.PluginParameter}]
-- True if this is a master plugin.
openmpt.Song.Plugin.master -> [boolean]
-- True if the dry signal is added back to the plugin mix.
openmpt.Song.Plugin.dry_mix -> [boolean]
-- True if the dry/wet ratio is expanded.
openmpt.Song.Plugin.expanded_mix -> [boolean]
-- True if auto-suspension is enabled for this plugin.
openmpt.Song.Plugin.auto_suspend -> [boolean]
-- The bypass state of the plugin.
openmpt.Song.Plugin.bypass -> [boolean]
-- The dry/wet ratio, ranging from 0 (100% wet / 0% dry) to 1 (0% wet / 100% dry).
-- When expanded_mix is true, the range is from from 0 (100% wet / -100% dry)
-- to 1 (-100% wet / 100% dry) instead.
openmpt.Song.Plugin.dry_wet_ratio -> [number]
-- The mix mode used for combining the dry and wet sound of the plugin.
openmpt.Song.Plugin.mix_mode -> [openmpt.Song.PluginMixMode]
-- Plugin post-gain factor.
openmpt.Song.Plugin.gain -> [number]
-- The plugin to which the output of this plugin is routed.
-- When reading this property, an openmpt.Song.Plugin is returned.
-- To set this property, either an openmpt.Song.Plugin object or a plugin slot
-- index (0 = no plugin, 1+ = plugin slots) or nil can be provided.
openmpt.Song.Plugin.output -> [openmpt.Song.Plugin or number]

-- Functions

-- Load a plugin into this plugin slot. The table passed as a parameter must be
-- either one of the entries returned by openmpt.app.registered_plugins(), or
-- can be created by the user. It must at least contain the library_name and id
-- entries.
openmpt.Song.Plugin:create(plugin_info) -> [boolean]

-- Sends a single MIDI message to the plugin. The message parameter must be a
-- table containing the individual bytes of the MIDI message as entries.
-- Returns true on success.
openmpt.Song.Plugin:midi_command(message) -> [boolean]

-- Hides or shows the plugin's editor window.
openmpt.Song.Plugin:toggle_editor() -> nil


--------------------------------------------------------------------------------
-- openmpt.Song.PluginParameter
--------------------------------------------------------------------------------

-- Properties

-- The index of the plugin parameter, e.g. 0 for the first parameter.
openmpt.Song.PluginParameter.index -> [read-only, number]
-- The parameter name as provided by the plugin.
openmpt.Song.PluginParameter.name -> [read-only, string]
-- The parameter value in [0.0, 1.0].
openmpt.Song.PluginParameter.value -> [number]

--------------------------------------------------------------------------------
-- openmpt.Song.PluginMixMode enum
--------------------------------------------------------------------------------

openmpt.Song.PluginMixMode.DEFAULT = "DEFAULT"
openmpt.Song.PluginMixMode.WET_SUBTRACT = "WET_SUBTRACT"
openmpt.Song.PluginMixMode.DRY_SUBTRACT = "DRY_SUBTRACT"
openmpt.Song.PluginMixMode.MIX_SUBTRACT = "MIX_SUBTRACT"
openmpt.Song.PluginMixMode.MIDDLE_SUBTRACT = "MIDDLE_SUBTRACT"
openmpt.Song.PluginMixMode.BALANCE = "BALANCE"
openmpt.Song.PluginMixMode.INSTRUMENT = "INSTRUMENT"

--------------------------------------------------------------------------------
-- openmpt.FadeLaw enum
--------------------------------------------------------------------------------

openmpt.FadeLaw.LINEAR = "LINEAR"
openmpt.FadeLaw.EXPONENTIAL = "EXPONENTIAL"
openmpt.FadeLaw.SQUARE_ROOT = "SQUARE_ROOT"
openmpt.FadeLaw.LOGARITHMIC = "LOGARITHMIC"
openmpt.FadeLaw.QUARTER_SINE = "QUARTER_SINE"
openmpt.FadeLaw.HALF_SINE = "HALF_SINE"

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

--TODO: callbacks enum

--------------------------------------------------------------------------------
-- openmpt.Song.NewNoteAction enum
--------------------------------------------------------------------------------

openmpt.Song.NewNoteAction.NOTE_CUT = "NOTE_CUT"
openmpt.Song.NewNoteAction.CONTINUE = "CONTINUE"
openmpt.Song.NewNoteAction.NOTE_OFF = "NOTE_OFF"
openmpt.Song.NewNoteAction.NOTE_FADE = "NOTE_FADE"

--------------------------------------------------------------------------------
-- openmpt.Song.DuplicateCheckType enum
--------------------------------------------------------------------------------

openmpt.Song.DuplicateCheckType.NONE = "NONE"
openmpt.Song.DuplicateCheckType.NOTE = "NOTE"
openmpt.Song.DuplicateCheckType.SAMPLE = "SAMPLE"
openmpt.Song.DuplicateCheckType.INSTRUMENT = "INSTRUMENT"
openmpt.Song.DuplicateCheckType.PLUGIN = "PLUGIN"

--------------------------------------------------------------------------------
-- openmpt.Song.DuplicateNoteAction enum
--------------------------------------------------------------------------------

openmpt.Song.DuplicateNoteAction.NOTE_CUT = "NOTE_CUT"
openmpt.Song.DuplicateNoteAction.NOTE_OFF = "NOTE_OFF"
openmpt.Song.DuplicateNoteAction.NOTE_FADE = "NOTE_FADE"

--------------------------------------------------------------------------------
-- openmpt.Song.FilterMode enum
--------------------------------------------------------------------------------

openmpt.Song.FilterMode.UNCHANGED = "UNCHANGED"
openmpt.Song.FilterMode.LOWPASS = "LOWPASS"
openmpt.Song.FilterMode.HIGHPASS = "HIGHPASS"

--------------------------------------------------------------------------------
-- openmpt.Song.ResamplingMode enum
--------------------------------------------------------------------------------

openmpt.Song.ResamplingMode.NEAREST = "NEAREST"
openmpt.Song.ResamplingMode.LINEAR = "LINEAR"
openmpt.Song.ResamplingMode.CUBIC_SPLINE = "CUBIC_SPLINE"
openmpt.Song.ResamplingMode.SINC_WITH_LOWPASS = "SINC_WITH_LOWPASS"
openmpt.Song.ResamplingMode.SINC = "SINC"
openmpt.Song.ResamplingMode.AMIGA = "AMIGA"
openmpt.Song.ResamplingMode.DEFAULT = "DEFAULT"

--------------------------------------------------------------------------------
-- openmpt.Song.PluginVolumeHandling enum
--------------------------------------------------------------------------------

openmpt.Song.PluginVolumeHandling.MIDI_VOLUME = "MIDI_VOLUME"
openmpt.Song.PluginVolumeHandling.DRY_WET_RATIO = "DRY_WET_RATIO"
openmpt.Song.PluginVolumeHandling.IGNORE = "IGNORE"

--------------------------------------------------------------------------------
-- Extended Lua Standard Library Functions
--
-- The OpenMPT scripting API adds custom functions to existing Lua standard
-- library tables.
-- These functions are available in addition to the standard Lua functions.
-- Other functions are removed for safety or require usage permissions to be
-- requested by the script (either in the script manifest or through
-- openmpt.app.request_permission). If a function is called without the
-- necessary permission, it throws an error.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- os (Operating System Library Extensions)
--------------------------------------------------------------------------------

-- Returns a table containing all directory names in the specified path.
-- Requires 'os.dirnames' permission.
-- path: The directory path to scan
os.dirnames(path) -> [{string}]

-- Returns a table containing all filenames in the specified path.
-- Requires 'os.filenames' permission.
-- path: The directory path to scan
-- filter: Optional file pattern filter (defaults to "*.*")
os.filenames(path [, filter]) -> [{string}]

-- Creates a directory at the specified path.
-- Requires 'os.mkdir' permission.
-- path: The directory path to create
-- Returns true on success, false otherwise.
os.mkdir(path) -> [boolean]

-- - The following standard os functions require the script to request
--   the respective permisions:
--   execute, getenv, remove, rename, tmpname.
--   The permissions are named accordingly (e.g. 'os.execute').
-- -  Note: The remove function does not require the os.remove permission if the
--   filename was returned by os.tmpname.
-- -  Note: Files created with os.tmpname are automatically deleted on exit.
-- - The exit function is not available.

--------------------------------------------------------------------------------
-- debug (Debug Library)
--------------------------------------------------------------------------------

-- The debug library is guarded behind the 'debug' permission.
-- It cannot be requested at runtime.
-- No functionality is otherwise added or removed.

--------------------------------------------------------------------------------
-- io (Input / Output Library)
--------------------------------------------------------------------------------

-- - The following standard io functions require the script to request
--   the respective permisions:
--   popen, tmpfile
--   The permissions are named accordingly (e.g. 'io.popen').
-- - Functions taking a filename (open, input, output, lines) require either:
--   - The file was explicitly requested to be opened by the user, e.g. through
--     openmpt.app.file_load_dialog.
--   - The 'allow_write_all_files' or 'allow_read_all_files' permission was
--     granted to the script.
--   - The filename was was returned by os.tmpname
-- - Note: Requesting a file to be written also allows it to be read, but not
--   vice versa.
-- - Note: Files created with io.tmpfile are automatically deleted on exit.


--------------------------------------------------------------------------------
-- package (Package Library)
--------------------------------------------------------------------------------

-- - The following standard io functions require the script to request
--   the respective permisions:
--   loadlib
--   The permissions are named accordingly (e.g. 'package.loadlib').

--------------------------------------------------------------------------------
-- Other
--------------------------------------------------------------------------------

-- load, dofile and loadfile are not available.

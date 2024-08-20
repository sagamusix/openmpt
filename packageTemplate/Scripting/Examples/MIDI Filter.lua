OPENMPT_SCRIPT =
{
   api_version = 1, -- The API version the script requires to run - REQUIRED!
   name = "MIDI Transpose",
   description = "Flips all the notes in a MIDI octave upside-down",
   author = "OpenMPT Devs"
}

function MIDIFilter(message)
	if(((message[1] & 0xF0) == 0x90) or ((message[1] & 0xF0) == 0x80)) then
		message[2] = (message[2] // 12) * 12 + (11 - message[2] % 12)
	end
	return message
end

print("To remove this filter, run openmpt.app.remove_midi_filter(" .. openmpt.app.register_midi_filter(MIDIFilter) .. ")")

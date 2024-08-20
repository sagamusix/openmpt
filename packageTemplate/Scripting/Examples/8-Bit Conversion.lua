OPENMPT_SCRIPT =
{
   api_version = 1, -- The API version the script requires to run - REQUIRED!
   name = "Convert all Samples to 8-Bit",
   author = "OpenMPT Devs"
}

mod = openmpt.active_song()

for k,smp in pairs(mod.samples) do
   if not smp.opl then
      smp.bits_per_sample = 8
	end
end

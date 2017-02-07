## Experiments with AVerMedia AVerTV Hybrid + FM Volar (A828) TV tuner

* working_pcap_from_win.txt 

	This is wireshark text export of USB traffic from host to TV tuner with 'Time Display Format - Seconds Since Previous Displayed Packet' selected.
	Traffic was captured using usbmon on linux host when TV tuner software was running in Windows on VirtualBox guest. Configured DVB-T channel 44 (658.00 MHz) 
	at 8MHz bandwidth.
	
* pcap_txt_to_a828_replay.sh
    
    Script converts the above into format suitable for replaying with a828_replay.
    
* a828_replay.c
    
    Sends input USB data in bulk_data_1.txt (alt setting 1, interrupts enabled) and bulk_data_6.txt (alt setting 6, streaming mode) to TV tuner. 
    Experiments show that bulk_data_1.txt may be skipped.

* bulk_data_6.txt
    
    Working test data, configures DVB-T stream on alt setting 6 (windows driver uses 0, 1, 6)
    Streams channel 44 (658.00 MHz) at 8MHz bandwidth, output goes to /tmp/out.ts
class TraktorKontrolX1MK1Class {
    constructor() {

        this.cmds = {
            GET_DEVICE_INFO: 0x1,
            READ_ERP: 0x2,
            READ_ANALOG: 0x3,
            READ_IO: 0x4,
            WRITE_IO: 0x5,
            MIDI_READ: 0x6,
            MIDI_WRITE: 0x7,
            AUDIO_PARAMS: 0x9,
            AUTO_MSG: 0xb,
            DIMM_LEDS: 0xc,
        };


        // LED index map
        this.leds = {
            "FX2_FILTER3": 1,
            "FX2_FILTER2": 2,
            "FX2_FILTER1": 3,
            "FX2_ON": 4,
            "FX1_FILTER3": 5,
            "FX1_FILTER2": 6,
            "FX1_FILTER1": 7,
            "FX1_ON": 8,
            "SYNC_R": 9,
            "PLAY_R": 10,
            "CUP_ABS_R": 11,
            "CUE_REL_R": 12,
            "BEAT_RIGHT_R": 13,
            "BEAT_LEFT_R": 14,
            "TEMPO_OUT_R": 15,
            "TEMPO_IN_R": 16,
            "TEMPO_OUT_L": 17,
            "TEMPO_IN_L": 18,
            "BEAT_RIGHT_L": 19,
            "BEAT_LEFT_L": 20,
            "CUP_ABS_L": 21,
            "CUE_REL_L": 22,
            "SYNC_L": 23,
            "PLAY_L": 24,
            "FX1_MASTER_L": 25,
            "FX2_SNAP": 26,
            "FX1_MASTER_R": 27,
            "FX2_QUANT": 28,
            "SHIFT": 29,
            "HOTCUE_YELLOW": 30,
            "HOTCUE_GREEN": 31,
        };

        // Brightness levels
        this.brightness = {
            "OFF": 0x00,
            "DIM": 0x0a,
            "ON": 0x7f
        };

        // Function to set LED state
        this.setLedState = function(ledName, brightness) {

            if (!(ledName in this.leds)) {
                console.log("Unknown LED:", ledName);
                return;
            }

            let index = this.leds[ledName];

            this.ledPacket[index] = brightness;
            controller.send(this.ledPacket);

            console.log(`LED ${ledName} set to ${brightness.toString(16)}`);
        };

        // Init function
        this.init = function(id, debug) {
            console.info("Initializing Traktor Kontrol X1 MK1 in bulk mode");

            console.log("Getting device info");
            let deviceInfoRequest = new Array(32).fill(0x0);
            deviceInfoRequest[0] = this.cmds["GET_DEVICE_INFO"];
            controller.send(deviceInfoRequest)

            console.log("Initializing LEDs");
            let ledStartPacket = new Array(32).fill(this.brightness["DIM"]);
            ledStartPacket[0] = this.cmds["DIMM_LEDS"]

            controller.send(ledStartPacket)
        };

        // Shutdown function
        this.shutdown = function() {
            console.info("Shutting down Traktor Kontrol X1 MK1");

            console.log("Turning off LEDs");
            let ledShutdownPacket = new Array(32).fill(this.brightness["OFF"]);
            ledStartPacket[0] = this.cmds["DIMM_LEDS"]
            controller.send(ledShutdownPacket)
        };

        // Incoming data handler
        this.incomingData = function(data, length) {
            console.log("Received Data:", data);
        };
    }

}

var TraktorKontrolX1MK1 = new TraktorKontrolX1MK1Class;

function Decoder(bytes, fPort)  {
    var decoded = {};

    if (fPort == 1) {  
        payloadversion = bytes[0];
      	decoded.length = bytes.length;
  
        // byte0: payloadversion
        // byte1: sensor status
        for (i = 2; i < bytes.length-1; i++) {
            switch (bytes[i]) {     
                case 0x01:
                    decoded.battery = (bytes[i+1] + 256) / 100.0;
                    i = i+1;
                    break;
                case 0x10:
                    decoded.temperature = ((bytes[i+1] & 0x80 ? 0xffff << 16 : 0) + (bytes[i+1] << 8) + bytes[i+2]) / 100.0;
                    i = i+2;
                    break;
                case 0x11:
                    decoded.humidity = bytes[i+1];
                    i = i+1;
                    break;
                case 0x12:
                    decoded.pressure = ((bytes[i+1] << 8) + bytes[i+2]) / 10.0;
                    i = i+2;
                    break;
                case 0x50:
                    decoded.pm25 = ((bytes[i+1] << 8) + bytes[i+2]) / 10.0;
                    i = i+2;
                    break;
                case 0x51:
                    decoded.pm10 = ((bytes[i+1] << 8) + bytes[i+2]) / 10.0;
                    i = i+2;
                    break;
            }
        }
    }
    return decoded;
}

function decodeUplink(input) {
    var data = input.bytes;
    var valid = true;

    if (typeof Decoder === "function") {
        data = Decoder(data, input.fPort);
    }

    if (valid) {
        return {
            data: data
        };
    } else {
        return {
            data: {},
            errors: ["Invalid data received"]
        };
    }
}

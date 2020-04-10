var recvBuffer = function(_playerconfig){
	this.player = new Player(_playerconfig);
	this.canvas = this.player.canvas;
	this.buffer = new Array();
	this.temp_chunk = new Array();
	this.chunk_list = new Array();
	var that = this;
	this.findFrame = function() {
		// console.log("Start finding frame");
		// console.log(that);


		if (that.temp_chunk.length > 0){
			// console.log(that.buffer)
			var i = 0;
			var first_nal_header;
			var frame_nals = new Array();
			var frame_count = 0;
			var starting_time = Date.now();
			var empty_counter = 0;
			// There is an issue: the last frame in buffer will not be decoded!!!!
			while (i < that.temp_chunk.length){
				if (that.temp_chunk[i] == 1){
					// Check whether i is 3 or 4
					if (i == 2){
						if (that.temp_chunk[0] == 0 && that.temp_chunk[1] == 0){
							first_nal_header = that.temp_chunk[i+1];
							if (first_nal_header == 101 || first_nal_header == 65){
								// Find a I or P or B frame
								frame_count += 1
							}
						}
					}
					else{
						if (that.temp_chunk[i-2] == 0 && that.temp_chunk[i-1] == 0){
							if (frame_count == 1){
								// A frame (one nal or multiply nals are found previously)
								// Return the frame nals
								// console.log("Starting of second nals is found, return ");
								// console.log(that.buffer.length);
								if(that.temp_chunk[i-3] == 0){
									frame_nals = that.temp_chunk.splice(0, i-3);
								}
								else{
									frame_nals = that.temp_chunk.splice(0, i-2);
								}
								break;
							}
							first_nal_header = that.temp_chunk[i+1];
							if (first_nal_header == 101 || first_nal_header == 65){
								// Find a I or P or B frame
								// console.log("A frame is found!")
								frame_count += 1
							}

						}
					}
				}
				i += 1;
			}

			// console.log(frame_nals);
			if (frame_count == 1){
				// var start_decode = Date.now();
				// console.log("start_decode" + start_decode);	
				if (frame_nals.length == 0){
					frame_nals = that.temp_chunk.splice(0,that.temp_chunk.length);
					// console.log(frame_nals);
					that.updateTempChunk();
				}
				var new_frame_nals = new Uint8Array(frame_nals);
				that.onDecodeMessage(new_frame_nals);	
				empty_counter = 0;
						
			}
			else{
				// console.log("Freezing, frame is not found");
				empty_counter += 1;
				
			}
		}
		else{
			// Buffer is empty.
			// console.log("Freezing, buffer is empty!")
		}
		// console.log(time_left);
		if(empty_counter >= 40){
			// Freezing too long
		}
		else{
			// var time_left = Math.max(100 - (Date.now() - starting_time), 0);
			var time_left = 40;
			setTimeout(that.findFrame, time_left);

		}
	};

}


recvBuffer.prototype.updateTempChunk = function() {
	// console.log(this.buffer.length);
	if (this.buffer.length > 0){
		var temp_len = this.chunk_list.pop();
		console.log(temp_len);
		this.temp_chunk = this.buffer.splice(0, temp_len);
	}
	// console.log(this.buffer.length);
}


recvBuffer.prototype.onReceivData = function(data) {
	var that = this
	// console.log(data.length);
	if (that.temp_chunk.length == 0){
		console.log("Buffer is empty!")
		data.forEach(function(v) {that.temp_chunk.push(v)}, that.temp_chunk);
	}
	else{
		console.log("There is buffer!")
		that.chunk_list.push(data.length);
		data.forEach(function(v) {that.buffer.push(v)}, that.buffer);
	}

	// console.log(data);
	// console.log(this.buffer.length);
	// console.log(data.length);
	// this.buffer.push.apply(this.buffer, data);
	// console.log(this.buffer.length);
	// First of all, parse the data
	// this.dataParser(data);

	// Then append data to buffer if it is data, or parse mpd

	// And make next action if a mpd is received

};


recvBuffer.prototype.dataParser = function(data){
	var msg = JSON.parse(data)
	switch(msg.type){
		case "mpd":

		case "video":
	}
}

recvBuffer.prototype.onDecodeMessage = function(nal) {
	this.player.decode(nal);
};



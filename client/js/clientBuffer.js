var recvBuffer = function(_playerconfig, fps, chunk_in_seg, startup){
	this.state = 0;
	this.speed_state = 0;		// 0: Normal, 1: Fast (1.1), -1: Slow (0.9)
	this.player = new Player(_playerconfig);
	this.canvas = this.player.canvas;
	this.buffer = new Array();
	this.fps = fps;
	this.buffer_length = 0.0;
	this.chunk_duration = 1000*chunk_in_seg/fps;
	this.startup = startup*1000;
	this.freezing = 0.0
	this.segReceiveTime = new Array();
	this.segReceiveIdx = new Array();
	this.segFrameidx = 0;
	this.frameCounter = 0;
	this.framesInChunk = 5;
	this.print_frame_counter = 0;
	this.print_seg_idx = 0;
	this.pre_lat = 0;
	// this.temp_chunk = new Array();
	// this.chunk_list = new Array();
	// var that = this;
	
	// this.adjustTimeInterval(200);

}

recvBuffer.prototype.findFrame = function() {
	var empty_counter = 0;
	var counter = 0; // for  pace debug
	// console.log("Start finding frame");
	// console.log(that);
	//var myTime = setTimeout(that.findFrame, 1000/that.fps);
	var frame_count = 0;
	var starting_time = new Date();
	var first_nal_header;
	var frame_nals;
	//console.log(starting_time.getMilliseconds());

	//console.log("this");
	if (this.buffer.length > 0){
		var i = 2;
		

		while (i < this.buffer.length) {
			if (this.buffer[i] == 1){
				// Check whether i is 3 or 4
				if (i == 2){
					if (this.buffer[0] == 0 && this.buffer[1] == 0){
						first_nal_header = this.buffer[i+1];
						if (first_nal_header == 101 || first_nal_header == 65){
							// Find a I or P or B frame
							frame_count += 1
						}
					}
				}
				else{
					if (this.buffer[i-2] == 0 && this.buffer[i-1] == 0){
						if (frame_count == 1){
							// A frame (one nal or multiply nals are found previously)
							// Return the frame nals
							// console.log("Starting of second nals is found, return ");
							// console.log(that.buffer.length);
							if(this.buffer[i-3] == 0){
								frame_nals = this.buffer.splice(0, i-3);
							}
							else{
								frame_nals = this.buffer.splice(0, i-2);
							}
							frame_count += 1;
							break;
						}
						first_nal_header = this.buffer[i+1];
						if (first_nal_header == 101 || first_nal_header == 65){
							// Find a I or P or B frame
							// console.log("A frame is found!")
							frame_count += 1
						}

					}
				}
			}
			i+=1;
		}

		if (frame_count == 2){
			// var start_decode = Date.now();
			// console.log("start_decode, time is: " + start_decode);
			// console.log("Get 1 effetive frame");		
			var new_frame_nals = new Uint8Array(frame_nals);
			this.onDecodeMessage(new_frame_nals);
			empty_counter = 0;
			this.buffer_length = Math.max(0, this.buffer_length-1000/this.fps);
		}
		else if (frame_count == 1){
			// ONly one frame in buffer
			// console.log("last frame in the buffer");
			var new_frame_nals = new Uint8Array(this.buffer.splice(0, this.buffer.length)); 
			this.onDecodeMessage(new_frame_nals);	
			empty_counter = 0
			// this.buffer_length = Math.max(0, this.buffer_length-1000/this.fps);
			this.buffer_length = 0;
		}
		else{
			// console.log("Freezing, frame is not found");

			empty_counter += 1;
			this.freezing += 1000/this.fps;
			clearInterval(this.loopFind);
			this.state = 0;
		}	
	}
	else{
		// Buffer is empty.
		// console.log("Freezing, buffer is empty!")
		empty_counter += 1;
		clearInterval(this.loopFind);
		this.state = 0;
	}
	// if(empty_counter >= 50){
	// 	Freezing too long
	// 	clearTimeout(myTime);
	// }
	// else{
	// 	var time_left = Math.max(1000/that.fps - (Date.now() - starting_time), 0);
	// 	// console.log("frame count is " + frame_count + " and time left is: " + time_left);
	// 	that.buffer_length -= 1000/that.fps;
	// 	that.buffer_length = Math.max(0, that.buffer_length);
	// 	setTimeout(that.findFrame, time_left);
	// }




}


recvBuffer.prototype.updateTempChunk = function() {
	if (this.buffer.length > 0){
		var temp_len = this.chunk_list.pop();
		this.temp_chunk = this.buffer.splice(0, temp_len);
	}
}

recvBuffer.prototype.updateInfo = function(){
	document.getElementById("buffer_length").value = this.buffer_length/1000;
	// console.log(this.buffer_length);
	setTimeout( () => { this.updateInfo()}, 250);
}

recvBuffer.prototype.onReceivData = function(data, chunks) {
	var that = this;
	// console.log(data);
	// that.buffer.push(...data);
	data.forEach(function(v) {that.buffer.push(v)}, that.buffer);
	this.buffer_length += this.chunk_duration * chunks;
	//this.frameCounter += chunks * framesInChunk;
	//this.frameCounter %= 25;
	this.freezing = 0.0;
	// console.log("state is " + this.state);


	if (this.state == 0 && this.buffer_length >= this.startup){
		this.state = 1;
		this.updateInfo();
		// this.findFrame();
		var t = this;
		this.adjustTimeInterval(1, 0);
		console.log("Starts to decode!");
	}
};

recvBuffer.prototype.onReceivSegTime = function(data, seg_idx) {
	// var that = this;
	// that.buffer.push(...data);
	this.segReceiveTime.push(...data);
	// if (this.segReceiveIdx.length >= 1)
		// console.assert(this.segReceiveIdx[this.segReceiveIdx.length - 1] +1 == seg_idx)
	this.segReceiveIdx.push(seg_idx);
	// console.log(this.segReceiveIdx);
	this.segFrameidx += this.fps;	
};

recvBuffer.prototype.updateDelay = function(){
	var start_time = this.segReceiveTime.splice(0,6);
	var seg_idx = this.segReceiveIdx.splice(0,1)
	// console.log(start_time)
	// this.segReceiveTime.shift();
	var min = start_time.shift();
	var sec = start_time.shift();
	var msec = new Uint32Array(start_time.reverse())[0];
	var d = new Date();
	this.pre_lat = (d.getMilliseconds() - msec) + (d.getMinutes() - min)*60*1000 + (d.getSeconds() - sec)*1000; //siquan
	document.getElementById("real_latency").value = Math.round(this.pre_lat)/1000;
	document.getElementById("seg_index").value = seg_idx;
}

recvBuffer.prototype.onDecodeMessage = function(nal) {
	this.segFrameidx--;
	this.player.decode(nal);
	if(this.segFrameidx % this.fps == 0) {
		this.updateDelay();
	}
	this.print_frame_counter += 1;
	if(this.print_frame_counter >= 25) {
		this.print_frame_counter = 0;
		this.print_seg_idx += 1;
	}
	//console.log(this.print_frame_counter + " " + this.print_seg_idx);
};

recvBuffer.prototype.adjustTimeInterval = function(multiply, speed_state) {  //use this to addjust the pace
	console.log("Speed changes to!!!!!!!!!!!!!!!!!!!!!!!!!\t", speed_state) ;
	clearInterval(this.loopFind);
	var t = this;
	var newfps = multiply * this.fps;
	console.log("new fps is " + newfps);
	this.loopFind = setInterval(function() {
    		t.findFrame();}, 1000/newfps);
	this.speed_state = speed_state;

}



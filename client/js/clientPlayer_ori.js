/* For all the functions here, includuing player/socket init. */
// import * as tf from '@tensorflow/tfjs';
// Init Websocket
function myPlayer(_playerconfig, server_addr, server_port, fps, target_latency, chunk_in_seg, startup){
	this.state = 0;
	this.socket;
	// this.loadModel(server_addr); // siquan 
	this.server_addr = server_addr;
	this.server_port = server_port;
	this.bitrates;
	this.br_idx;
	this.seg_idx;
	this.chunk_idx;
	this.buffer = new recvBuffer(_playerconfig, fps, chunk_in_seg, startup);
	this.target_lat = target_latency;
	this.pre_lat = 0.0;
	this.chunk_in_seg = chunk_in_seg;
	this.pre_time_recording = 0.0;
	this.message_time_recording = 0.0;
	this.pre_bw = 0.0;
	this.state_obv = this.state_init();
	this.server_wait = 0.0;
	// var output;
	this.qref = 2;
	this.m = 0
	this.last_buffer = 0.0
	this.counter = 0
	this.state_len = 0
	// var myDate = new Date();
	// 	var mm = myDate.getMinutes();     //获取当前分钟数(0-59)
	// 	var ss = myDate.getSeconds();     //获取当前秒数(0-59)
	// 	var ms = myDate.getMilliseconds();    //获取当前毫秒数(0-999)
	// 	console.log("start time is "+ mm + ":" + ss + ":" + ms);
	this.loadModel(server_addr); // siquan 
	this.chunk_size = 0;
	this.upload_flag = 0;
	

	//this.connectionInfo = navigator.connection
	//this.getBW = 
	 //navigator.connection.addEventListener('change', this.getBW);
	// this.getBW();

	this.ws = new WebSocket("ws://10.7.5.101:33333");
	// this.ws = new WebSocket("ws://192.168.1.179:33333");
	this.ws.onopen = function(evt) { 
  		console.log("Connection open ..."); 
  		//ws.send(this.pre_bw);
	};

}

// myPlayer.prototype.getBW = function() {
// 	console.log(navigator.connection);
// }

myPlayer.prototype.state_init = function() {
	var state_arr = new Array(S_INFO);
	for (var i = 0; i < S_INFO; i++) {
		state_arr[i] = new Array(15).fill(0);
	}
	return state_arr;
}

myPlayer.prototype.loadModel = async function(server_addr) {
	//console.log(tf)
	const MODEL_URL = 'http://' + server_addr + '/web_model/weights_manifest.json';
	const FROZEN = 'http://' + server_addr + '/web_model/tensorflowjs_model.pb';
	this.model = await tf.loadGraphModel(FROZEN, MODEL_URL);
	//console.log(this.model);
	await this.clientSocket();
	//const model = await tf.loadGraphModel(MODEL_URL);
	//console.log(model.predict(arr))
	//console.log(this.model);
	//this.model.dispose();
	//this.model.execute();
	//return model;
}

myPlayer.prototype.update_state_arr = function (datasize, download_dur, chunk_dur, chunk_num, br_idx){
	// shift the first element from recording
	for (var i = 0; i < S_INFO; i++) {
		this.state_obv[i].shift();
	}
	// Then update with new data
	this.state_obv[0].push(Math.min(datasize/150000,1)); 											// data size in Mbits
	this.state_obv[1].push(download_dur/1000);												// download duration
	this.state_obv[2].push((this.buffer.buffer_length + chunk_dur)/1000/5);					// chunk time duration
	this.state_obv[3].push(Math.log(this.bitrates[br_idx]/this.bitrates[0])/3.0);														// number of chunks
	//this.state_obv[4].push(this.bitrates[br_idx]/this.bitrates[0]);							//bitrate ratio 
	this.state_obv[4].push(0);																// sync
	this.state_obv[5].push(this.buffer.state/2.0);
	this.state_obv[6].push(Math.max(chunk_num*this.buffer.chunk_duration - download_dur, 0.0)/200);		// server wait				
	this.state_obv[7].push(this.buffer.freezing/1000/3.0);
	this.state_len = Math.min(S_LEN, this.state_len+1)										// freezing
	// console.log(this.state_obv);
}



myPlayer.prototype.calculateBW = function(data_size){
	// console.log(this.pre_time_recording, this.message_time_recording);		
	console.assert(this.message_time_recording >= this.pre_time_recording);
	if (this.message_time_recording == this.pre_time_recording) {
		this.pre_bw = 0; // upper bound, 1 Gbps, in kbps
	} else {
		this.pre_bw = 8 * data_size / (this.message_time_recording - this.pre_time_recording); // in kbps
	}
	this.chunk_size = data_size * 8 / 1000; // kbits
	// console.log(this.message_time_recording, this.pre_time_recording , (this.message_time_recording - this.pre_time_recording), 8 * data_size, this.pre_bw);
	// this.pre_time_recording = 0.0;
	// this.message_time_recording = 0.0;
}

myPlayer.prototype.rl_choose_rate = function() {
	// console.log(this.model);
	output = this.model.execute({['actor/InputData/X']: tf.tensor([this.state_obv])}, 
	'actor/actor_output/Softmax').dataSync();
	//choose rate using the softmax output
	
	var cumsum = new Array(A_DIM).fill(0);

	for (var i=1;i<A_DIM;i++) {
	cumsum[i] = output[i-1] + cumsum[i-1];
	}
	const rdm = Math.random();
	var br = 1;
	// Has to modify here,
	while (br < A_DIM) {			// Real encoding 4 bitrates
	if (cumsum[br] > rdm) {
	break;
	}
	br += 1;
	}
	this.br_idx = br-1;
	this.br_idx = Math.min(this.br_idx, 3)
	console.log("chose bit rate is " + this.br_idx);
	//this.br_idx = 0;

	// this.new_PI_choose_rate();
}

myPlayer.prototype.requestSeg = function(){
	// if (this.seg_idx%10 == 0) {
	// 	this.br_idx += 1;
	// 	this.br_idx = (this.br_idx%3);
	// }

	// Introduce Tensorflow here to choose the rate
	this.rl_choose_rate();

	//this.seg_idx+= 1;
	//this.chunk_idx = 0;


	this.socket.send(JSON.stringify({
		type: 'SEG_REQ', seg_idx: this.seg_idx, br_idx: this.br_idx
		// type: 'SEG_REQ', seg_idx: 0, br_idx: this.br_idx
	}));
	console.log(JSON.stringify({
		type: 'SEG_REQ', seg_idx: this.seg_idx, br_idx: this.br_idx
	}))

	// this.ws.send("seg idx " + this.seg_idx + "\tbr_idx" + this.br_idx);

	// Adjust speed
	// Hard coding latency changing
	if (this.buffer.state == 1 ){
		console.log("latency is " + document.getElementById("real_latency").value);
		if (document.getElementById("real_latency").value >= 4)  {
			if(this.buffer.speed_state != 1) {
				this.buffer.adjustTimeInterval(1.2, 1); // modify by siquan
			}			
			
		}
		else if (document.getElementById("real_latency").value <= 3) {
			if(this.buffer.speed_state == 1) {
				this.buffer.adjustTimeInterval(1, 0); // modify by siquan
			}
			
		}
	}
}

myPlayer.prototype.processHeader = function(header){
	// console.log(header);
	var type = (header[0] & 0xF0) >> 4;
	var br_idx = header[0] & 0x0F;
	var seg_idx = new Uint16Array(header.slice(1,3))[0];
	var chunk_num = 0;
	var chunk_start;
	var sendRequest = 0;
	var get_time = 0;
	// console.log("Receive seg, and processHeader, type is "+ type);
	if (type == 0){
		console.log("Type zero data!");
	} else if (type == 1) {
		console.assert(0 == 1);
		// Directly request for next seg
		this.message_time_recording = Date.now()
		this.seg_idx += 1;
		this.chunk_idx = 0;
		// this.requestSeg();
		sendRequest = 1;
		chunk_num = this.chunk_in_seg;
		// start_time = new Uint64Array(header.slice(4,12))[0];
		get_time = 1;

	} else if (type == 2 || type == 3 ) {
		// console.log(header[3]);
		chunk_start = (header[3] & 0xF0) >> 4;
		chunk_num = header[3] & 0x0F;
		// console.log(chunk_start, this.chunk_idx);
		// console.assert(chunk_start == this.chunk_idx);
		if (chunk_start == 0)
			get_time = 1;
		// start_time = new Uint64Array(header.slice(4,12))[0];
		this.message_time_recording = Date.now()
		// console.log("CHunk start at: " + chunk_start + " and number is " + chunk_num + " chunk_idx " + this.chunk_idx);
		this.chunk_idx += chunk_num;
		// console.log("receive chunks " + this.chunk_idx + " and it should be " + this.chunk_in_seg);
		if (this.chunk_idx >= this.chunk_in_seg) {
			this.seg_idx += 1;
			this.chunk_idx = 0;
			sendRequest = 1;
			// this.requestSeg();
			// setTimeout(this.requestSeg(), 300);
		}
		// if(type == 2) {
		// 	sendRequest = 1;
		// }
	} else if (type == 4) {
		// console.log("receive 4 type");
		chunk_start = (header[3] & 0xF0) >> 4;
		// console.log(br_idx, this.br_idx);
		// console.log(seg_idx, this.seg_idx);
		// console.log(chunk_start, this.chunk_idx);
		// if (br_idx == this.br_idx && seg_idx == this.seg_idx && chunk_start == this.chunk_idx) {
			// valid ping packet
			this.pre_time_recording = Date.now();
		// }
	}
	return [type, sendRequest, chunk_num, get_time, br_idx];
}

myPlayer.prototype.clientSocket = function(){
	// console.log("in socket");
	var that = this;
	// var state = 0;
	// console.log(this, that)
	that.socket = new WebSocket('ws://' + this.server_addr + ':' + this.server_port);
	that.socket.binaryType = 'arraybuffer';
	// Connection opened
	that.socket.addEventListener('open', function (event) {
		that.InitReq();
		// that.InitReq();
	});

	// Listen for messages
	that.socket.addEventListener('message', function (event) {
		// console.log(event.data);
		// that.onDecodeMessage(newdata);

		if(event.data instanceof ArrayBuffer) {
			// console.log("Received seg data in ArrayBuffer");
			var newdata = Array.from(new Uint8Array(event.data));
			var header = newdata.splice(0, 4);
			var headerRet = that.processHeader(header);
			// console.log("head[1] is " + headerRet[1]);
			if (headerRet[0] == 4 ) {
				// It is a ping request
			} else {
				// Message is received, should calculate bw
				var download_duration = that.message_time_recording - that.pre_time_recording;
				that.update_state_arr(newdata.length + 4, 
						download_duration, 
						headerRet[2]*that.buffer.chunk_duration,
						headerRet[2],
						headerRet[4]);

				that.calculateBW(newdata.length + 4);
				if (headerRet[3] == 1) {
					// Get encoding time, 6 bytes
					// that.calculateLat(newdata.splice(0,6));
					that.onRecvSegTime(newdata.splice(0,6), that.seg_idx);
					// console.log(header.length);
				}
				if (headerRet[1]) {
					// Should send out request
					that.updateBWInfo();
					sleep(0).then(() => {
						that.requestSeg();
					});
				}
				that.onRecvChunk(newdata, headerRet[2]);
			}
			// Check buffer length and display it when it exceed threshold
		} else if (event.data instanceof Blob) {
			console.log("Received seg data in blob");
		} else{
			// console.log("text received");
			var reply = JSON.parse(event.data);
			console.log(reply.type);
			if (reply.type == 'INIT REPLY'){
				that.InitializePara(reply);
				// that.updateBWInfo();
			} else if (reply.type == 'SEG REPLY') {
				// that.get_seg_data(reply);
				console.log("Should not happen!");
			}
		}
	
	});

	that.socket.addEventListener('close', function (event) {
		console.log('Websocket closed!');
	});
}

myPlayer.prototype.onDecodeMessage = function(data) {
	this.buffer.player.decode(data);
};

myPlayer.prototype.onRecvChunk = function(data, chunks) {
	// console.log(data.length)
	this.buffer.onReceivData(data, chunks);
	this.frame_counter += 1;
};

myPlayer.prototype.onRecvSegTime = function(data, seg_idx) {
	// console.log(data)
	this.buffer.onReceivSegTime(data, seg_idx);
};

myPlayer.prototype.getCanvas = function() {
	return this.buffer.player.canvas;
};

myPlayer.prototype.InitReq = function(){
	// console.log("ENter init req");
	this.socket.send(JSON.stringify({
	  type: 'INIT'
	}));
}

myPlayer.prototype.InitializePara = function(reply){
	this.bitrates = reply.bitrates;
	this.br_idx = 0;
	this.seg_idx = Math.max(0, parseInt(reply.curr_idx) - this.target_lat);
	this.chunk_idx = 0;
	sleep(1500).then(() => {
		this.requestSeg();
	});
	// Add debug flag
	//this.playSeg_idx = this.seg_idx;
	this.buffer.print_frame_counter = 0;
	this.buffer.print_seg_idx = this.seg_idx;
	this.upload(); // upload delay buffer bw data
}

myPlayer.prototype.updateBWInfo = function(){
	document.getElementById("real_bandwidth").value = this.pre_bw / 1000;  //Math.round(this.pre_bw)/1000;
	// const connection = window.navigator.connection;
	// console.log("downlink" + connection.downlink);
	// console.log(connection);
	//this.getBW();
	// setTimeout( () => { this.updateBWInfo()}, 00);
}

myPlayer.prototype.calculateLat = function(start_time){
	// console.log(start_time[0],start_time[1],start_time[2],start_time[3],start_time[4],start_time[5]);
	// var min = start_time.shift();
	// var sec = start_time.shift();
	// var msec = new Uint32Array(start_time.reverse())[0];
	// var d = new Date();
	// this.pre_lat = (d.getMilliseconds() - msec) + (d.getMinutes() - min)*60*1000 + (d.getSeconds() - sec)*1000 + this.buffer.buffer_length; //siquan
	// document.getElementById("real_latency").value = Math.round(this.pre_lat)/1000;
}

function sleep (time) {
  return new Promise((resolve) => setTimeout(resolve, time));
}

myPlayer.prototype.upload = function() {


	var myDate = new Date();
	var mm = myDate.getMinutes();     //获取当前分钟数(0-59)
	var ss = myDate.getSeconds();     //获取当前秒数(0-59)
	var ms = myDate.getMilliseconds();    //获取当前毫秒数(0-999)
	this.ws.send(this.buffer.print_seg_idx + " "+ mm + ":" + ss + ":" + ms +" " + this.seg_idx + " " + this.chunk_idx+ " " + this.chunk_size + " " + this.br_idx+" "+ document.getElementById("buffer_length").value +" " + document.getElementById("real_bandwidth").value + " " + document.getElementById("real_latency").value);

	
	//this.ws.send("buffer   \t" + document.getElementById("buffer_length").value);
	//this.ws.send("bandwidth\t" + document.getElementById("real_bandwidth").value);
	//this.ws.send("latancy  \t" + document.getElementById("real_latency").value);
	setTimeout( () => { this.upload()}, 100);
}

// Following is for 360 rendering

// myPlayer.prototype.canvas_init = function(){
// 	var mesh;
// 	// var container;

// 	// container = document.getElementById( 'container' );

// 	camera = new THREE.PerspectiveCamera( 75, window.innerWidth / window.innerHeight, 1, 1100 );
// 	camera.target = new THREE.Vector3( 0, 0, 0 );

// 	scene = new THREE.Scene();

// 	var geometry = new THREE.SphereBufferGeometry( 500, 60, 40 );
// 	// invert the geometry on the x-axis so that all of the faces point inward
// 	geometry.scale(-1, 1, 1);


// 	// var canvas1 = document.getElementById('canvas');
// 	var texture = new THREE.CanvasTexture(this.canvas)

// 	texture.minFilter = THREE.LinearFilter;
// 	texture.format = THREE.RGBFormat;

// 	var material = new THREE.MeshBasicMaterial( { map : texture } );

// 	mesh = new THREE.Mesh( geometry, material );

// 	scene.add( mesh );

// 	renderer = new THREE.WebGLRenderer();
// 	renderer.setPixelRatio( window.devicePixelRatio );
// 	renderer.setSize( window.innerWidth, window.innerHeight );
// 	// container.appendChild( renderer.domElement );

// 	document.addEventListener( 'mousedown', onDocumentMouseDown, false );
// 	document.addEventListener( 'mousemove', onDocumentMouseMove, false );
// 	document.addEventListener( 'mouseup', onDocumentMouseUp, false );
// 	document.addEventListener( 'wheel', onDocumentMouseWheel, false );
// 	window.addEventListener( 'resize', onWindowResize, false );

// 	function onWindowResize() {
// 		camera.aspect = window.innerWidth / window.innerHeight;
// 		camera.updateProjectionMatrix();
// 		renderer.setSize( window.innerWidth, window.innerHeight );

// 	}

// 	function onDocumentMouseDown( event ) {
// 		event.preventDefault();
// 		isUserInteracting = true;
// 		onPointerDownPointerX = event.clientX;
// 		onPointerDownPointerY = event.clientY;
// 		onPointerDownLon = lon;
// 		onPointerDownLat = lat;

// 	}

// 	function onDocumentMouseMove( event ) {
// 		if ( isUserInteracting === true ) {
// 			lon = ( onPointerDownPointerX - event.clientX ) * 0.1 + onPointerDownLon;
// 			lat = ( onPointerDownPointerY - event.clientY ) * 0.1 + onPointerDownLat;
// 		}
// 	}

// 	function onDocumentMouseUp( event ) {
// 		isUserInteracting = false;

// 	}

// 	function onDocumentMouseWheel( event ) {
// 		distance += event.deltaY * 0.05;
// 		distance = THREE.Math.clamp( distance, 1, 50 );

// 	}

// 	function update() {

// 		lat = Math.max(-85, Math.min(85, lat));
// 		phi = THREE.Math.degToRad(90-lat);
// 		theta = THREE.Math.degToRad(lon);

// 		// // console.log(lon), record the lon history
// 		// if (FovHistory.length >= fovlength){
// 		// 	FovHistory.shift();
// 		// }
// 		// FovHistory.push([lon, Math.round(video1.currentTime * 100)/ 100]);

// 		camera.position.x = distance * Math.sin(phi) * Math.cos(theta);
// 		camera.position.y = distance * Math.cos(phi);
// 		camera.position.z = distance * Math.sin(phi) * Math.sin(theta);

// 		camera.lookAt(camera.target);

// 		var geometry = new THREE.SphereBufferGeometry(500, 60, 40);
// 		geometry.scale(-1, 1, 1);

// 		// var canvas = document.getElementById('myCanvas');
// 		var texture = new THREE.CanvasTexture(this.canvas);
// 		var material   = new THREE.MeshBasicMaterial({map:texture});

// 		var scene = new THREE.Mesh(geometry, material);

// 		renderer.render(scene, camera);
// 		geometry.dispose();
//         geometry = undefined;

//         material.dispose();
//         material = undefined;

//         texture.dispose();
//         texture = undefined;
// 	}
// }

// myPlayer.prototype.testingPlay = function(){
// 	console.log(this)
// 	var img = document.getElementById('nier');
// 	var context;
// 	// var c = document.getElementById('myCanvas');
// 	if (this.player.webgl){
// 		context = this.player.canvas.getContext('webgl');
// 		console.log(context);
// 		context.drawImage(img, 10, 10);
// 	} else {
// 		console.log(this.player.canvas);
// 		context = this.player.canvas.getContext('2d');
// 		console.log(context);
// 		context.drawImage(img, 10, 10);
// 	}
// }

// myPlayer.prototype.draw = function(){
// 	var context = this.player.canvas.getContext('2d');
// 	context.drawImage(this.video, this.lon, this.lat, this.weight, this.height);
// 	update();
// 	setTimeout(draw,20);
// }




myPlayer.prototype.HM = function() { // naive av
	var result = 0;
	for (var i = S_LEN - this.state_len; i < S_LEN; i++) {
		result += 1 / (this.state_obv[0][i]/this.state_obv[1][i]);
	}
	//result /= n;
	result =  this.state_len / result;
	return result;

	// return this.state_obv[0][S_LEN-1]/this.state_obv[1][S_LEN-1]
}


myPlayer.prototype.naive_choose_rate = function(state) {
	var result = HM();
	var br = choose_idx_by_rate(result);
	this.br_idx = br;

}

myPlayer.prototype.choose_idx_by_rate = function(rate) {
	var br = 0;
	while (br < TEST_A_DIM - 1) {			// Real encoding 4 bitrates
		if(rate > this.bitrates[br]) {
			br++;
		} else {
			break;
		}
	}
	return br;
}

myPlayer.prototype.PI_choose_rate = function() {
	var m;
	var arrlen = this.state_obv.length;
	var counter = 0;
	var threshold = qref; // target buffer len
	var Fk = fluctuate(); // fluctuate factor F(k)
	var Ttk = HM(); // estimated bit rate
	var vk = Fk * Ttk; // estimated  fluctuate bit rate (Mbps)
	if(this.buffer.buffer_length < threshold / 2) {
		var last_real_br = this.state_obv[0][this.state_obv.length-1] /  this.state_obv[1][this.state_obv.length-1];
		br = choose_idx_by_rate(last_real_br);
	} else if(vk > this.bitrates[this.br_idx]) { //  -v(k) > v(k - 1)
		counter++;
		m = calculate_m();
		if(counter > m) {
			br = choose_idx_by_rate(HM());
			counter = 0;
		}
	} else if(vk < this.bitrates[this.br_idx]) {
		counter = 0;
	} else {
		br = this.br_idx; // remain last bit rate
	}
	this.br_idx = br;
}

myPlayer.prototype.new_PI_choose_rate = function() {
	// siquan comment all last_buffer
	if (this.state_len == 0) {
		this.br_idx = 0
		return
	}
	var tunned_buffer = Math.max(this.buffer.buffer_length/1000 - this.buffer.freezing/1000, 0.0);
	if (this.buffer.freezing > 0) {
		this.qref += 0.5 * this.buffer.buffer_length;
	}
	// console.log(tunned_buffer,this.qref)
	if (tunned_buffer < this.qref * 0.5) {
		this.br_idx = 0;
		 this.last_buffer = this.buffer.buffer_length/1000;

	} else if (tunned_buffer < this.qref * 0.75){
		var temp_rate = this.choose_idx_by_rate(this.HM()*1000);
		if (temp_rate > this.br_idx ){
			self.br_idx += 1;
		} else {
			self.br_idx = temp_rate;
		}
		 this.last_buffer = this.buffer.buffer_length/1000;
	} else {
		var Fk = this.fluctuate()
		var ave_bw = this.HM()*1000
		var tuned_bw = ave_bw * Fk;
		var m = this.calculate_m();
		// console.log(Fk, tuned_bw, m)
		if (tuned_bw > this.bitrates[this.br_idx]){
			this.counter += 1
			if (this.counter > m) {
				this.counter = 0
				this.br_idx = this.choose_idx_by_rate(tuned_bw)
				 this.last_buffer = this.buffer.buffer_length/1000
			} 
		} else {
			self.counter = 0
		}
		 this.last_buffer = this.buffer.buffer_length/1000
	}
}

myPlayer.prototype.fluctuate = function() {
	var p  = 0.6;
	var t =  Math.exp(p * (this.buffer.buffer_length/1000 - this.qref));
	var fq = 2 * t / (1 + t);
	var ft = 1 / (1 - (this.buffer.buffer_length/1000 - this.last_buffer))
	var fv = 1.0;

	return fq * ft * fv; 
}

myPlayer.prototype.calculate_m = function() {
	var delta  = (this.state_obv[2][this.state_obv.length-1]/ -  this.state_obv[2][this.state_obv.length-2]) / 1000;
	
	if (delta >= 0.2) {
		return 1
	} else if (delta >= 0.1) {
		return 2
	} else {
		return 3
	}
}

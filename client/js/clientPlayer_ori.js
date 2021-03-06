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
	this.br_idx = 0;
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
	


	// iLQR variables
	this.ilqr_len = 5;
	this.n_iteration = 10;
	this.w1 = 1
    this.w2 = 1
    this.w3 = 1 
    this.w4 = 1
    this.w5 = 1
    this.barrier_1 = 1
    this.barrier_2 = 1
    this.delta = 0.2
    // this.n_step
    this.predicted_bw;
    this.predicted_rtt; 
    this.n_iteration = 50
    this.Bu 
    this.b0
    this.r0
    this.target_buffer
    this.states = []
    this.rates = []
    this.step_size = 0.2
    this.lat_data = ''

	//this.connectionInfo = navigator.connection
	//this.getBW = 
	 //navigator.connection.addEventListener('change', this.getBW);
	// this.getBW();

	this.ws = new WebSocket("ws://192.168.1.195:33333");
	// this.ws = new WebSocket("ws://192.168.1.179:33333");
	this.ws.onopen = function(evt) { 
  		console.log("Connection open ..."); 
  		//ws.send(this.pre_bw);
	};


}


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
	

	/////////////////////////////////////////////////////////////////////////////////////////
	// var myDate1 = new Date()
	// var ss1 = myDate1.getSeconds();     
	// var ms1 = myDate1.getMilliseconds();   
	// Only enabled when uusing DRL choose rate
	// this.model = await tf.loadGraphModel(FROZEN, MODEL_URL);
	// var myDate2 = new Date();
	// var ss2 = myDate2.getSeconds();     
	// var ms2 = myDate2.getMilliseconds();   
	// console.log(ms2, ms1)
	// this.lat_data = 'drl model ' + (ss2 - ss1) + ':' + (ms2 - ms1)
	/////////////////////////////////////////////////////////////////////////////////////////

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
	// console.log("chose bit rate is " + this.br_idx);
	//this.br_idx = 0;

	// this.new_PI_choose_rate();
}

myPlayer.prototype.requestSeg = function(){
	console.log("requestSeg");
	// Introduce Tensorflow here to choose the rate
	
	this.lat_data = ''   
	var t1 = performance.now()
	/////////////////////////////////////////////////////////////////////////////////////////
	// change choose
	// this.naive_choose_rate();
	this.new_PI_choose_rate();
	// this.rl_choose_rate();
	// this.iLQR_choose_rate();
	/////////////////////////////////////////////////////////////////////////////////////////
	var t2 = performance.now()

	this.lat_data = (t2 - t1)
	//this.seg_idx+= 1;
	//this.chunk_idx = 0;


	this.socket.send(JSON.stringify({
		type: 'SEG_REQ', seg_idx: this.seg_idx, br_idx: this.br_idx
		// type: 'SEG_REQ', seg_idx: 0, br_idx: this.br_idx
	}));
	// console.log(JSON.stringify({
	// 	type: 'SEG_REQ', seg_idx: this.seg_idx, br_idx: this.br_idx
	// }))

	// this.ws.send("seg idx " + this.seg_idx + "\tbr_idx" + this.br_idx);

	// Adjust speed
	// Hard coding latency changing


	// if (this.buffer.state == 1 ) {
	// 	// console.log("latency is " + document.getElementById("real_latency").value);
	// 	if (document.getElementById("real_latency").value >= 4 && this.buffer.buffer_length > 1000)  {
	// 		if(this.buffer.speed_state != 1) {
	// 			this.buffer.adjustTimeInterval(1.2, 1); // modify by siquan
	// 		}			
			
	// 	}
	// 	else if (document.getElementById("real_latency").value <= 3.5 || this.buffer.buffer_length <= 1000) {
	// 		if(this.buffer.speed_state == 1) {
	// 			this.buffer.adjustTimeInterval(1, 0); // modify by siquan
	// 		}
			
	// 	}
	// }
	
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
	sleep(100).then(() => {
		this.requestSeg();
	});
	// Add debug flag
	//this.playSeg_idx = this.seg_idx;
	this.buffer.print_frame_counter = 0;
	this.buffer.print_seg_idx = this.seg_idx;
	// this.upload(); // upload delay buffer bw data
	var t = this;

	// send metrics
	this.uploaddata = setInterval(function() {
		t.upload();}, 1000);
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
myPlayer.prototype.upload_lat = function() {
}


myPlayer.prototype.upload = function() {
	// var myDate = new Date();
	// var mm = myDate.getMinutes();     //获取当前分钟数(0-59)
	// var ss = myDate.getSeconds();     //获取当前秒数(0-59)
	// var ms = myDate.getMilliseconds();    //获取当前毫秒数(0-999)
	// this.ws.send(this.buffer.print_seg_idx + " "+ mm + ":" + ss + ":" + ms +" " + this.seg_idx + " " + this.chunk_idx+ " " + this.chunk_size + " " + this.br_idx+" "+ document.getElementById("buffer_length").value +" " + document.getElementById("real_bandwidth").value + " " + document.getElementById("real_latency").value);

	this.ws.send(this.lat_data)

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

function multiply(a, b) {
  var aNumRows = a.length, aNumCols = a[0].length,
      bNumRows = b.length, bNumCols = b[0].length,
      m = new Array(aNumRows);  // initialize array of rows
  for (var r = 0; r < aNumRows; ++r) {
    m[r] = new Array(bNumCols); // initialize the current row
    for (var c = 0; c < bNumCols; ++c) {
      m[r][c] = 0;             // initialize the current cell
      for (var i = 0; i < aNumCols; ++i) {
        m[r][c] += a[r][i] * b[i][c];
      }
    }
  }
  return m;
}

function sumArrayElements(){
        var arrays= arguments, results= [], 
        count= arrays.length, r= arrays[0].length, c = arrays[0][0].length;
        // console.log(arrays, r, c)

        for (var i=0;i<r;i++){
        	var row = []
        	for (var j=0;j<c;j++){
        		var s = 0;
        		for (var k=0;k<count;k++){
        			s += arrays[k][i][j]
        		}
        		row.push(s)
        	}
        	results.push(row)
        }
        return results;
    }

function minus(){
        var arrays= arguments, results= [], 
        count= arrays.length, r= arrays[0].length, c = arrays[0][0].length;
        // console.log(arrays, r, c)

        for (var i=0;i<r;i++){
        	var row = []
        	for (var j=0;j<c;j++){
        		var s = arrays[0][i][j] - arrays[1][i][j]
        		row.push(s)
        	}
        	results.push(row)
        }
        return results;
    }

function transpose(mat) { 
	var new_mat = [];
	for (var i= 0; i < mat[0].length;i++){
		// for each col of ori mat
		var row = []
		for (var j=0;j<mat.length;j++){
			// for each row of ori mat
			row.push(mat[j][i])
		}
		new_mat.push(row)
	}
	return new_mat
    // for (var i = 0; i < mat.length; i++) { 
    //     for (var j = 0; j < i; j++) { 
    //         const tmp = mat[i][j]; 
    //         mat[i][j] = mat[j][i]; 
    //         mat[j][i] = tmp; 
    //     } 
    // } 
    // return mat
}

function get_part(matrix, r_i, row, c_i, col){
       var mat = [];
       for(var i=r_i; i<row; i++){
       	var r = [];
       	for (var j=c_i; j < col; j++){
          r.push(matrix[i][j]);
       	}
       	mat.push(r)
       }
       return mat;
    }

function get1d(matrix, r_i, row){
	var mat = []
	for (var i=r_i;i<row;i++){
		mat.push(matrix[i])
	}
	return mat
}





myPlayer.prototype.update_matrix = function(step_i) {

	curr_state = this.states[step_i]
        curr_u = this.rates[step_i]
        bw = this.predicted_bw[step_i]
        rtt = 0.02
        b = curr_state[0]
        r = curr_state[1]
        u = curr_u
        f_1 = 100*(b-u/bw-rtt + (5-1)*this.delta)
        f_2 = b-u/bw-rtt+5*this.delta
        f_3 = 100*(b-u/bw-rtt + 5*this.delta-this.Bu)

        ce_power = (b-u/bw-rtt + (5-1)*this.delta)
        ce_power_1 = Math.min(-50*(u-0.1), -1)
        ce_power_2 = 50*(u-6.15)
        ce_power_terminate = (b-u/bw-rtt + 5*this.delta - 2 + 0.2)

        approx_e_f1 = Math.pow(Math.E,f_1)
        approx_e_f3 = Math.pow(Math.E,f_3)

   
    // console.log(f_1, f_2, f_3)
    // console.log(ce_power, ce_power_1, ce_power_2, ce_power_terminate)

    this.ft = [[(100*approx_e_f1/Math.pow((approx_e_f1+1),2))*(this.Bu*approx_e_f3+f_2)/(approx_e_f3+1) + 
		((this.Bu*100*approx_e_f3+approx_e_f3+1-100*approx_e_f3*f_2)/Math.pow((approx_e_f3+1),2))*approx_e_f1/(approx_e_f1+1)-
		100*this.delta*approx_e_f1/Math.pow((approx_e_f1+1),2),
	                0, 
	                -100*approx_e_f1*(this.Bu*approx_e_f3+f_2)/(bw*Math.pow((approx_e_f1+1),2)*(approx_e_f3+1)) + 
	                (approx_e_f1/(approx_e_f1+1))*(-100*this.Bu*approx_e_f3-approx_e_f3-1+100*approx_e_f3*f_2)/(bw*Math.pow((approx_e_f3+1),2)) + 
	                (100*this.delta*approx_e_f1)/(bw*Math.pow((approx_e_f1+1),2))],
	               [0, 0, 1]]

		approx_power0 = 4*(ce_power+1)
        approx_power1 = 15*ce_power+3.2
        approx_power2 = 20*(ce_power+0.2)
        approx_power3 = 10*(ce_power+3)
        approx_power4 = 5*(ce_power+1.5)
        action_ratio = -1/(bw)

        approx_e_0 = Math.pow(Math.E,approx_power0)
        approx_e_1 = Math.pow(Math.E,approx_power1)
        approx_e_2 = Math.pow(Math.E,approx_power2)
        approx_e_3 = Math.pow(Math.E,approx_power3)
        approx_e_4 = Math.pow(Math.E,approx_power4)

        delta_b = this.w3*(-1/2*(4*approx_e_0*(1+approx_e_1)-2*15*approx_e_1*approx_e_0)/Math.pow((1+approx_e_1),3) +
                         10*-1*Math.pow((1+approx_e_2),-2)*20*approx_e_2 + 
                         28*-1*Math.pow((1+approx_e_3),-2)*10*approx_e_3 + 
                         20*-1*Math.pow((1+approx_e_4),-2)*5*approx_e_4)
        delta_u = action_ratio*delta_b

        delta_bb = this.w3*(-1/2*((16*approx_e_0*(1+approx_e_1) + 
                            4*approx_e_0*15*approx_e_1 - 
                            30*(15*approx_e_1*approx_e_0 + 4*approx_e_0*approx_e_1))*(1+approx_e_1)- 
                            3*15*approx_e_1*(4*approx_e_0*(1+approx_e_1)-2*15*approx_e_1*approx_e_0))/
                            Math.pow((1+approx_e_1),4) - 
                            200*Math.pow((1+approx_e_2),-2)*20*approx_e_2*(-2*Math.pow((1+approx_e_2),-1)*approx_e_2+1) - 
                            280*Math.pow((1+approx_e_3),-2)*10*approx_e_3*(-2*Math.pow((1+approx_e_3),-1)*approx_e_3+1) - 
                            100*Math.pow((1+approx_e_4),-2)*5*approx_e_4*(-2*Math.pow((1+approx_e_4),-1)*approx_e_4+1))
        delta_bu = action_ratio*delta_bb
        delta_uu = Math.pow(action_ratio,2)*delta_bb

	    this.ct = transpose([[delta_b, this.w2*2*Math.log(r/u)/r, 
                     this.w1*-1/u + this.w2*2*Math.log(u/r)/u -
                     50*this.barrier_1*Math.pow(Math.E,ce_power_1) +
                     50*this.barrier_2*Math.pow(Math.E,ce_power_2) + delta_u]])


        this.CT = transpose([[delta_bb, 0, delta_bu],
                            [0, this.w2*2*(1-Math.log(r/u))/Math.pow(r,2), -2*this.w2/(u*r)],
                            [delta_bu, this.w2*-2/(u*r), 
                             this.w1/Math.pow(u,2) + this.w2*2*(1-Math.log(u/r))/Math.pow(u,2) +
                             2500.0*this.barrier_1*Math.pow(Math.E,ce_power_1) + 
                             2500.0*this.barrier_2*Math.pow(Math.E,ce_power_2) + delta_uu]])
}


myPlayer.prototype.iterate_LQR = function() {
    var VT = 0
    var vt = 0
    // console.log("ENter iterate")
    // console.log(this.states)
    // console.log(this.rates)
    for (var i = 0; i < this.n_iteration;i++){

    	var converge = 1,
    	KT_list = [],
        kt_list = [],
        VT_list = [],
        vt_list = [],
        pre_xt_list = [],
        new_xt_list = [],
        pre_ut_list  = [],
        d_ut_list = [];

    	for (var j = 0;j<this.ilqr_len;j++){

    		KT_list.push(0)
            kt_list.push(0)
            VT_list.push(0)
            vt_list.push(0)
            pre_xt_list.push(0)
            new_xt_list.push(0)
            pre_ut_list.push(0)
            d_ut_list.push(0)
    	}
    
    	for (var step_i = this.ilqr_len -1; step_i >=0; step_i--){
        	this.update_matrix(step_i);
        	// console.log(this.ft, this.ct, this.CT)
            var xt = [[this.states[step_i][0]],[this.states[step_i][1]]]
            var ut = [[this.rates[step_i]]]
            pre_xt_list[step_i] = xt
            pre_ut_list[step_i] = ut[0][0]
            if (step_i == this.ilqr_len-1){
                Qt = this.CT
                qt = this.ct
            }
            else {
            	// console.log(this.ft, transpose(this.ft), VT )
                Qt = sumArrayElements(this.CT, multiply(multiply(transpose(this.ft), VT), this.ft))
                qt = sumArrayElements(this.ct, multiply(transpose(this.ft), vt))
                }       

            Q_xx = get_part(Qt, 0, 2, 0, 2)  
            Q_xu = get_part(Qt, 0, 2, 2, 3)
            Q_ux = get_part(Qt, 2, 3, 0, 2)
            Q_uu = get_part(Qt, 2, 3, 2, 3) 
            // console.log(qt)  
            q_x = get1d(qt, 0, 2)
            q_u = get1d(qt, 2, 3)
            // console.log(Q_xx, Q_xu, Q_ux, Q_uu, q_x, q_u)  
            // console.log()
            KT = multiply([[-1]], multiply([[1/Q_uu[0][0]]], Q_ux))
            // console.log(KT)
            kt = multiply([[-1]], multiply([[1/Q_uu[0][0]]], q_u))    
            // console.log(kt)                       
            // d_u = sumArrayElements(multiply(KT, xt), kt)
            // console.log(d_u)
            // console.log(Q_xx)
            // // console.log(Q_xu, KT)
            // console.log(multiply(Q_xu, KT))
            // console.log(multiply(transpose(KT), Q_ux))
            // // console.log(transpose(KT), Q_uu)
            // // console.log(multiply(transpose(KT), Q_uu))
            // // console.log(KT)
            // console.log(multiply(multiply(transpose(KT), Q_uu), KT))
            // console.log(multiply(transpose(KT), Q_ux))
            // console.log(multiply(transpose(KT), Q_ux))

            VT = sumArrayElements(Q_xx, multiply(Q_xu, KT), multiply(transpose(KT), Q_ux), multiply(multiply(transpose(KT), Q_uu), KT))
            vt = sumArrayElements(q_x, multiply(Q_xu, kt), multiply(transpose(KT), q_u), multiply(multiply(transpose(KT), Q_uu), kt))
            // console.log(VT)
            // console.log(vt)
            // d_ut_list[step_i] = d_u
            KT_list[step_i] = KT
            kt_list[step_i] = kt
            VT_list[step_i] = VT
            vt_list[step_i] = vt

    	}
    	// console.log(KT_list)


    	// Then forward pass 
    	new_xt_list[0] = pre_xt_list[0]
        for (var step_i = 0; step_i < this.ilqr_len; step_i++) {
            // console.log(new_xt_list[step_i], pre_xt_list[step_i])
            d_x = minus(new_xt_list[step_i], pre_xt_list[step_i])
            // console.log(d_x)
            k_t = kt_list[step_i]
            K_T = KT_list[step_i]
            // console.log(k_t, K_T)
            d_u = sumArrayElements(multiply(K_T, d_x), k_t)
            // console.log(d_u)
            new_u = pre_ut_list[step_i] + this.step_size*d_u[0]

            if (converge == 1 && Math.round((new_u + Number.EPSILON) * 100) / 100  !=  Math.round((this.rates[step_i] + Number.EPSILON) * 100) / 100 ){
                converge = 0 
            }

            this.rates[step_i] =  Math.round((new_u + Number.EPSILON) * 100) / 100
            new_x = new_xt_list[step_i]       
            rtt = 0.02
            bw = this.predicted_bw[step_i]

            new_next_b = this.sim_fetch(new_x[0][0], new_u, rtt, bw)     
            if (step_i < this.ilqr_len - 1){
                new_xt_list[step_i+1] = [[new_next_b], [new_u]]
                this.states[step_i+1] = [Math.round((new_next_b + Number.EPSILON) * 100) / 100, this.rates[step_i]]
            }
                
            else {
                this.states[step_i+1] = [Math.round((new_next_b + Number.EPSILON) * 100) / 100, this.rates[step_i]]
            }
            // console.log(this.states)
            // console.log(this.rates)
		}
        if (converge == 1){
        	// console.log("converged at step ", step_i)
            break
        }

    }  
    r_idx = this.translate_to_rate_idx()
    return r_idx   
}

myPlayer.prototype.translate_to_rate_idx = function() {
        var first_action = this.rates[0]
        //distance = [np.abs(first_action-br/KB_IN_MB) for br in BITRATE]
        //rate_idx = distance.index(min(distance))
        var rate_idx = 0;
        for (var j=4; j>=0;j--){
            if (this.bitrates[j]/1000 <= first_action){
            	rate_idx = j
            	break
            }
        }
        console.log("choose rate, ", rate_idx)
        return rate_idx
}


myPlayer.prototype.iLQR_choose_rate = function() {
	this.set_x0();
	this.predicted_bw = this.HM_iLQR();
	this.generate_initial_x(this.predicted_bw[0])
	this.br_idx = this.iterate_LQR();
}

myPlayer.prototype.generate_initial_x = function(i_rate) {
	console.log('init state for ilqr')
	this.rates = []
	for (var i = 0; i < this.ilqr_len; i++) {
	    this.rates.push(i_rate);
	}

	this.states = [];
	this.states.push([this.b0, this.r0])
	// console.log(this.states)
	for (var r_idx = 0; r_idx < this.ilqr_len;r_idx ++){
	    x = this.states[r_idx]
	    u = this.rates[r_idx]
	    bw = this.predicted_bw[r_idx]
	    new_b = this.sim_fetch(x[0], u, 0.02, bw)
	    new_x = [new_b, u]
	    this.states.push(new_x)
	}

}

myPlayer.prototype.set_x0 = function() {
	this.b0 = this.buffer.buffer_length/1000;
	this.r0 = this.bitrates[this.br_idx]/1000;
	this.Bu = this.buffer.pre_lat/1000
}


myPlayer.prototype.HM_iLQR = function() { // naive av
	var result = Math.min(this.HM(), 2.5);
	return [result, result, result, result, result]
}

myPlayer.prototype.sim_fetch = function(buffer_len, seg_rate, rtt, bw) { // naive av
	// console.log(buffer_len, seg_rate, rtt, bw)
	seg_size = seg_rate
    freezing = 0.0
    wait_time = 0.0
    current_reward = 0.0
    download_time = seg_size/bw + rtt

    freezing = Math.max(0.0, download_time - buffer_len - 4*0.2)
    buffer_len = Math.max(buffer_len - download_time + 4*0.2, 0.0)
    buffer_len += 0.2
    buffer_len = Math.min(this.Bu, buffer_len)
    return buffer_len 
}

myPlayer.prototype.HM = function() { // naive av
	var result = 0;
	if (this.state_len == 0 ){
		return 0.3
	}
	for (var i = S_LEN - this.state_len; i < S_LEN; i++) {
		result += 1 / (this.state_obv[0][i]/this.state_obv[1][i]);
		// console.log("rate is " + (this.state_obv[0][i]/this.state_obv[1][i])  + " datasize is " + this.state_obv[0][i] + " time is " + this.state_obv[1][i]);
	}
	//result /= n;
	result =  this.state_len / result;
	console.log("result is " + result);
	return result;

	// return this.state_obv[0][S_LEN-1]/this.state_obv[1][S_LEN-1]
}


myPlayer.prototype.naive_choose_rate = function(state) {

	var result = this.HM();
	//console.log("result is " + result);
	var br = this.choose_idx_by_rate(result * 1000);
	this.br_idx = br;

}

myPlayer.prototype.choose_idx_by_rate = function(rate) {
	var br = 0;

	while (br < TEST_A_DIM - 1) {			// Real encoding 4 bitrates
		// console.log("in A_DIM" + br)
		//console.log("bit rate is " + this.bitrates[br]);
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
	var threshold = this.qref; // target buffer len
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
			this.br_idx += 1;
		} else {
			this.br_idx = temp_rate;
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
			this.counter = 0
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

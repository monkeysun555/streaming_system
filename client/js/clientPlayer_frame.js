/* For all the functions here, includuing player/socket init. */

// Init Websocket
function myPlayer(_playerconfig, server_addr, server_port){
	this.server_addr = server_addr;
	this.server_port = server_port;

	// this.buffer = new recvBuffer(_playerconfig);
	// this.started = 0;
	this.player = new Player(_playerconfig);
}


myPlayer.prototype.onDecodeMessage = function(data) {
	// var currentTime = Date.now();
	// console.log("Start Decoding: " + currentTime);

	console.log(data.length);

	this.player.decode(data);
};

myPlayer.prototype.onRecvChunk = function(data) {
	this.buffer.onReceivData(data);
};

myPlayer.prototype.getCanvas = function() {
	return this.player.canvas;
};


myPlayer.prototype.clientSocket = function(){
	var that = this;
	// console.log(this, that)
	socket = new WebSocket('ws://' + this.server_addr + ':' + this.server_port);
	socket.binaryType = 'arraybuffer';

	// Connection opened
	socket.addEventListener('open', function (event) {
		// that.buffer.grab();
	});

	// Listen for messages
	socket.addEventListener('message', function (event) {
		that.onDecodeMessage(new Uint8Array(event.data));
		/* For chunk */
		// var newdata = Array.from(new Uint8Array(event.data));
		// that.onRecvChunk(newdata);
		// that.buffer.findFrame();
	});

	socket.addEventListener('close', function (event) {
		console.log('Websocket closed!');
	});
}


myPlayer.prototype.canvas_init = function(){
	var mesh;
	// var container;

	// container = document.getElementById( 'container' );

	camera = new THREE.PerspectiveCamera( 75, window.innerWidth / window.innerHeight, 1, 1100 );
	camera.target = new THREE.Vector3( 0, 0, 0 );

	scene = new THREE.Scene();

	var geometry = new THREE.SphereBufferGeometry( 500, 60, 40 );
	// invert the geometry on the x-axis so that all of the faces point inward
	geometry.scale(-1, 1, 1);


	// var canvas1 = document.getElementById('canvas');
	var texture = new THREE.CanvasTexture(this.canvas)

	texture.minFilter = THREE.LinearFilter;
	texture.format = THREE.RGBFormat;

	var material = new THREE.MeshBasicMaterial( { map : texture } );

	mesh = new THREE.Mesh( geometry, material );

	scene.add( mesh );

	renderer = new THREE.WebGLRenderer();
	renderer.setPixelRatio( window.devicePixelRatio );
	renderer.setSize( window.innerWidth, window.innerHeight );
	// container.appendChild( renderer.domElement );

	document.addEventListener( 'mousedown', onDocumentMouseDown, false );
	document.addEventListener( 'mousemove', onDocumentMouseMove, false );
	document.addEventListener( 'mouseup', onDocumentMouseUp, false );
	document.addEventListener( 'wheel', onDocumentMouseWheel, false );
	window.addEventListener( 'resize', onWindowResize, false );

	function onWindowResize() {
		camera.aspect = window.innerWidth / window.innerHeight;
		camera.updateProjectionMatrix();
		renderer.setSize( window.innerWidth, window.innerHeight );

	}

	function onDocumentMouseDown( event ) {
		event.preventDefault();
		isUserInteracting = true;
		onPointerDownPointerX = event.clientX;
		onPointerDownPointerY = event.clientY;
		onPointerDownLon = lon;
		onPointerDownLat = lat;

	}

	function onDocumentMouseMove( event ) {
		if ( isUserInteracting === true ) {
			lon = ( onPointerDownPointerX - event.clientX ) * 0.1 + onPointerDownLon;
			lat = ( onPointerDownPointerY - event.clientY ) * 0.1 + onPointerDownLat;
		}
	}

	function onDocumentMouseUp( event ) {
		isUserInteracting = false;

	}

	function onDocumentMouseWheel( event ) {
		distance += event.deltaY * 0.05;
		distance = THREE.Math.clamp( distance, 1, 50 );

	}

	function update() {

		lat = Math.max(-85, Math.min(85, lat));
		phi = THREE.Math.degToRad(90-lat);
		theta = THREE.Math.degToRad(lon);

		// // console.log(lon), record the lon history
		// if (FovHistory.length >= fovlength){
		// 	FovHistory.shift();
		// }
		// FovHistory.push([lon, Math.round(video1.currentTime * 100)/ 100]);

		camera.position.x = distance * Math.sin(phi) * Math.cos(theta);
		camera.position.y = distance * Math.cos(phi);
		camera.position.z = distance * Math.sin(phi) * Math.sin(theta);

		camera.lookAt(camera.target);

		var geometry = new THREE.SphereBufferGeometry(500, 60, 40);
		geometry.scale(-1, 1, 1);

		// var canvas = document.getElementById('myCanvas');
		var texture = new THREE.CanvasTexture(this.canvas);
		var material   = new THREE.MeshBasicMaterial({map:texture});

		var scene = new THREE.Mesh(geometry, material);

		renderer.render(scene, camera);
		geometry.dispose();
        geometry = undefined;

        material.dispose();
        material = undefined;

        texture.dispose();
        texture = undefined;
	}
}

myPlayer.prototype.testingPlay = function(){
	console.log(this)
	var img = document.getElementById('nier');
	var context;
	// var c = document.getElementById('myCanvas');
	if (this.player.webgl){
		context = this.player.canvas.getContext('webgl');
		console.log(context);
		context.drawImage(img, 10, 10);
	} else {
		console.log(this.player.canvas);
		context = this.player.canvas.getContext('2d');
		console.log(context);
		context.drawImage(img, 10, 10);
	}
}

myPlayer.prototype.draw = function(){
	var context = this.player.canvas.getContext('2d');
	context.drawImage(this.video, this.lon, this.lat, this.weight, this.height);
	update();
	setTimeout(draw,20);
}
myPlayer.prototype.rl_choose_rate = function() {
	// console.log(return l);
	output = return l.execute({['actor/InputData/X']: tf.tensor([this.state_obv])}, 
								'actor/FullyConnected_5/Softmax').dataSync();
	// choose rate using the softmax output
	
	var cumsum = new Array(A_DIM).fill(0);
	for (var i=1;i<A_DIM;i++) {
		cumsum[i] = output[i-1] + cumsum[i-1];
	}
	const rdm = Math.random();
	var br = 1;
	// Has to modify here,
	while (br < TEST_A_DIM) {			// Real encoding 4 bitrates
		if (cumsum[br] > rdm) {
			break;
		}
		br += 1;
	}
	this.br_idx = br-1; // should be 0 --siquan
	// console.log(this.br_idx);

}


myPlayer.prototype.avbitrate = function() { // naive av
	var arrlen = this.state_obv.length;
	var pick_n_state = 10;
	var result = 0;
	var real_pick;
	if (arrlen > = pick_n_state) {
		real_pick = pick_n_state;
	} else {
		real_pick = arrlen;
	}
	for (var i = arrlen - real_pick; i < arrlen; i++) {
		result += 1 / this.state_obv[4][i] * this.bitrates[0];
	}
	//result /= n;
	result =  n / result;
	return result;
}


myPlayer.prototype.naive_choose_rate = function(state) {
	var result = avbitrate();
	var br = choose_idx_by_rate(result);
	this.br_idx = br;

}

myPlayer.prototype.choose_idx_by_rate = function(rate) {
	var br = 0；
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
	var Ttk = avbitrate(); // estimated bit rate
	var vk = Fk * Ttk; // estimated  fluctuate bit rate (Mbps)
	if(this.buffer.buffer_length < threshold / 2) {
		var last_real_br = this.state_obv[0][this.state_obv.length-1] /  this.state_obv[1][this.state_obv.length-1];
		br = choose_idx_by_rate(last_real_br);
	} else if(vk > this.bitrates[this.br_idx]) { //  -v(k) > v(k - 1)
		counter++;
		m = calculate_m();
		if(counter > m) {
			br = choose_idx_by_rate(avbitrate());
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
	var tunned_buffer = Math.max(this.buffer.buffer_length/1000 - this.buffer.freezing/1000, 0.0);
	if (tunned_buffer < this.qref * 0.5) {
		this.br_idx = 0;
		this.last_buffer = this.buffer.buffer_length/1000;

	} else if (tunned_buffer < this.qref * 0.75){
		var temp_rate = this.choose_idx_by_rate(this.avbitrate()*this.bitrates[0]);
		if (temp_rate > this.br_idx ){
			self.br_idx += 1;
		} else {
			self.br_idx = temp_rate;
		}
		this.last_buffer = this.buffer.buffer_length/1000;
	} else {
		var Fk = this.fluctuate()
		var tunned_bw = this.avbitrate() * Fk;
		var m = this.calculate_m();
		if (tunned_bw > this.bitrates[this.br_idx]){
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
	var t =  Math.exp(p * (this.buffer.buffer_length - this.qref)/1000);
	var fq = 2 * t / (1 + t);
	var ft = 1000 / (1000 - (this.buffer.buffer_length/1000 - this.last_buffer))
	var fv = 1.0;

	return fq * ft * fv; 
}

myPlayer.prototype.calculate_m = function() {
	var delta  = (this.state_obv[8][this.state_obv.length-1]/ -  this.state_obv[8][this.state_obv.length-2]) / 1000;
	
	if (delta >= 0.2) {
		return 1
	} else if (delta >= 0.1) {
		return 2
	} else {
		return 3
	}
}


// new from ssh
 
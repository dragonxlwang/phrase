class Eva:

	def __init__(self):
		self.freqfilename = './data/wiki.nyt.freq'
	def get_freq(self):
		term_wikifreq = {}
		term_nytfreq = {}
		with open(self.freqfilename) as fin:
			for line in fin:
				term, fn, fw = line[:-1].split('\t')
				term_wikifreq[term] = int(fw)
				term_nytfreq[term] = int(fn)
		return term_wikifreq, term_nytfreq

	def get_groundtruth(self, term_wikifreq, term_nytfreq, wikifreq_threshold, nytfreq_threshold):
		splitter = '=||='
		groundtruth = set()
		for term, freq in term_wikifreq.iteritems():
			if freq >= wikifreq_threshold and term_nytfreq[term] >= nytfreq_threshold:
				groundtruth.add(term.replace(' ', splitter))
		return groundtruth

	def eval(self, filename, wikifreq_threshold=100, nytfreq_threshold=200):
		term_wikifreq, term_nytfreq = self.get_freq()
		groundtruth = self.get_groundtruth(term_wikifreq, term_nytfreq, wikifreq_threshold, nytfreq_threshold)
		correct = 0
		pnum = 0
		gnum = len(groundtruth)
		with open(filename) as fin:
			for line in fin:
				pnum += 1
				if line.strip() in groundtruth:
					correct += 1
		p = float(correct) / float(pnum)
		r = float(correct) / float(gnum)
		f = 2 * p * r / (p + r)
		return p, r, f

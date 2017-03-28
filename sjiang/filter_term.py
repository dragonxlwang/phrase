import io, os, sys

terms = {}

def filter_word(word, lower):
	if lower:
		word = word.lower()
	return ''.join([x for x in word if x.isalpha() or x == '-']).strip()

def filter_file(filename, wordnumThreshold, wordlenThreshold, lower):
	fin = open(filename)
	istitle = 'wiki.title' in filename
	for line in fin:
		sp = line[:-1].split('\t')
		for i in range(len(sp) - 1):
			if i == 1:
				continue
			ori_words = sp[i].split(' ')
			words = []
			valid = True
			for word in ori_words:
				if ':' in word:
					valid = False
					break
				word = filter_word(word, lower)
				if len(word) > wordlenThreshold:
					valid = False
					break
				if word != '':
					words.append(word) 
			if not valid:
				continue
			if len(words) < 2 or len(words) > wordnumThreshold:
				continue
			term = ' '.join(words)
			if istitle:
				terms[term] = terms.get(term, 0) + 1
			else:
				terms[term] = terms.get(term, 0) + int(sp[-1])
	fin.close()

def output_result(filename):
	sort = sorted(terms.items(), key = lambda x:x[1], reverse = True)
	fout = open(filename, 'w')
	for term, freq in sort:
		fout.write('\t'.join([term, str(freq)]))
		fout.write('\n')
	fout.close()

if __name__=='__main__':
	filenames = ['/home/sjiang18/data/plans/wiki.anchor', '/home/sjiang18/data/plans/wiki.title']
	wordnumThreshold = 5
	wordlenThreshold = 20
	lower = True
	for filename in filenames:
		filter_file(filename, wordnumThreshold, wordlenThreshold, lower)
	outfilename = '/home/sjiang18/data/plans/wiki.term'
	output_result(outfilename)

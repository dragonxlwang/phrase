import io, os, sys

terms = set()
def filter_word(word, length):
	word = word.lower()
	return ''.join([x for x in word if x.isalpha() or x == '-']).strip()

def filter_file(filename, wordnumThreshold, wordlenThreshold):
	fin = open(filename)
	for line in fin:
		words = line[:-1].split('\t')[0].split(' ')
		if len(words) > wordnumThreshold:
			continue
		valid = True
		for i in range(len(words)):
			length = len(words[i])
			if length > wordlenThreshold or ':' in words[i]:
				valid = False
				break
			words[i] = filter_word(words[i], length)
		if not valid:
			continue
		term = ' '.join([word for word in words if word != '']).strip()
		if term == '':
			continue
		terms.add(term)
	fin.close()

def output_result(filename):
	fout = open(filename, 'w')
	for t in terms:
		fout.write(t)
		fout.write('\n')
	fout.close()

if __name__=='__main__':
	filenames = ['/home/sjiang18/Data/plans/wiki.anchor', '/home/sjiang18/Data/plans/wiki.title']
	wordnumThreshold = 5
	wordlenThreshold = 20
	for filename in filenames:
		filter_file(filename, wordnumThreshold, wordlenThreshold)
	outfilename = '/home/sjiang18/Data/plans/wiki.term'
	output_result(outfilename)

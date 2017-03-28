class TreeNode:
	def __init__(self):
		self.children = []
		self.childrenDic = {}
		self.length = 0
	def addChild(self, word):
		if word in self.childrenDic:
			return self.children[self.childrenDic[word]]
		self.childrenDic[word] = len(self.children)
		child = TreeNode()
		self.children.append(child)
		return child
	def getChild(self, word):
		if word not in self.childrenDic:
			return None
		return self.children[self.childrenDic[word]]

class PrefixTree:
	def __init__(self):
		self.root = TreeNode()
	def addTerm(self, term):
		words = [s for s in term.split(' ') if s != '']
		node = self.root
		for word in words:
			node = node.addChild(word)
		node.length = len(words)
	def startsWith_isTerm(self, words):
		node = self.root
		for i in range(len(words)):
			node = node.getChild(words[i])
			if node == None:
				return False, False
		return True, len(words) == node.length
		
	def getTerm(self, sentence):
		words = [s for s in sentence.split(' ') if s != '']
		terms = []
		'''for i in range(len(words) - 1):
			for j in range(i+1, len(words)):
				startsWith, isTerm = self.startsWith_isTerm(words[i: j + 1]) 
				if not startsWith:
					break
				if isTerm:
					terms.append(' '.join(words[i:j+1]))
		'''
		for i in range(len(words) - 1):
			node = self.root
			for j in range(i, len(words)):
				node = node.getChild(words[j])
				if node == None:
					break
				if j - i + 1 == node.length:
					terms.append(' '.join(words[i:j+1]))
		return terms

if __name__=='__main__':
	terms = ['white house', 'new york times', 'new york']
	s = 'new york times publish a new article about white house'
	tree = PrefixTree()
	for term in terms:
		tree.addTerm(term)
	tms = tree.getTerm(s)
	print tms

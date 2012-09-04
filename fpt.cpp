/* fpt.c
 *
 * Program Input:
 *	A configuration file consisting of 6 parameters
 *	1. User specified maximum size of itemset to be mined
 *	   If this value is not larger than zero or 
 *	   is greater than the greatest transaction size in the DB,
 *	   then it will be set to the greatest transaction size.
 *	2. Normalized support threshold, range: (0, 1]
 *	3. Total number of different items in the DB
 *	4. Total number of transactions in the DB
 *	5. Data file name
 *	6. Result file name for storing the large itemsets
 *
 * Program Output: do it later
 *
 */ 

#include<stdio.h>
#include<stdlib.h>
#include<string>
#include<time.h>
#include<vector>
#include<sstream>
#include<iostream>
using namespace std;
/***** Data Structure *****/
/* Description:
 *	Each node of an FP-tree is represented by a 'FPnode' structure.
 *	Each node contains an item ID, count value of the item, and
 *	node-link as stated in the paper.
 *	
 */
typedef struct FPnode *FPTreeNode;	/* Pointer to a FP-tree node */

typedef struct Childnode *childLink;	/* Pointer to children of a FP-tree node */

/*
 * Children of a FP-tree node
 */
typedef struct Childnode {
	FPTreeNode node;	/* A child node of an item */
	childLink next;		/* Next child */
} ChildNode;

/*
 * A FP-tree node
 */
typedef struct FPnode {
        int item;		/* ID of the item.  
				   Value of ID is within the range [0, m-1]
				   where m is the total number of different items in the database. */
        int count;		/* Value of count of the item.
				   This is the number of transactions containing items in the portion
				   of the path reaching this node. */
	int numPath;  		/* Number of leaf nodes in the subtree
			           rooted at this node.  It is used to
				   check whether there is only a single path 
				   in the FPgrowth function. */
	FPTreeNode parent;	/* Pointer to parent node */
        childLink children;	/* Pointer to children */
        FPTreeNode hlink;	/* Horizontal link to next node with same item */
} FPNode;


/*
 * A list to store large itemsets in descending order of their supports.
 * It stores all the itemsets of supports >= threshold.
 */
typedef struct Itemsetnode *LargeItemPtr;
typedef struct Itemsetnode {
	int support;
	int *itemset;
	LargeItemPtr next;
} ItemsetNode;

/***** Global Variables *****/
LargeItemPtr *largeItemset;	/* largeItemset[k-1] = array of large k-itemsets */
int *numLarge;			/* numLarge[k-1] = no. of large k-itemsets found. */
int *support1;			/* Support of 1-itemsets */
int *largeItem1;		/* 1-itemsets */

FPTreeNode root=NULL;		/* Initial FP-tree */
FPTreeNode *headerTableLink;	/* Corresponding header table */

int expectedK;			/* User input upper limit of itemset size to be mined */
int realK;			/* Actual upper limit of itemset size can be mined */
int threshold;			/* User input support threshold */
int numItem;			/* Number of items in the database */
int numTrans;			/* Number of transactions in the database */
char dataFile[100];		/* File name of the database */
char outFile[100];		/* File name to store the result of mining */

static int total_leaf_node = 0; /*test_tree use*/
int branch_i = 0;		/*test_tree use*/
int branch_j = 0;		/*test_tree use*/ 
/******************************************************************************************
 * Function: destroyTree
 *
 * Description:
 *	Destroy the FPtree rooted by a node and free the allocated memory.
 *	For each tree node being visited, all the child nodes
 *	are destroyed in a recursive manner before the destroy of the node.
 *
 * Invoked from:	
 * 	destroy()
 * 
 * Input Parameter:
 *	node	-> Root of the tree/subtree to be destroyed.
 */
void destroyTree(FPTreeNode node)
{
 childLink temp1, temp2;

 if (node == NULL) return;

 temp1 = node->children;
 while(temp1 != NULL) {
	temp2 = temp1->next;
	destroyTree(temp1->node);
	free(temp1);
	temp1 = temp2;
 }

 free(node);

 return;
}


/******************************************************************************************
 * Function: destroy
 *
 * Description:
 *	Free memory of following variables.
 *	- largeItemset
 *	- numLarge
 *	- headerTableLink
 *	- root
 *
 * Invoked from:	
 * 	main()
 * 
 * Functions to be invoked:
 *	destroyTree()	-> Free memory from the FP-tree, root.
 *
 * Global variables (read only):
 *	- realK
 */
void destroy()
{
 LargeItemPtr aLargeItemset; 
 int i;

 for (i=0; i < realK; i++) {
	aLargeItemset = largeItemset[i];
	while (aLargeItemset != NULL) {
		largeItemset[i] = largeItemset[i]->next;
		free(aLargeItemset->itemset);
		free(aLargeItemset);
		aLargeItemset = largeItemset[i];
	}
 }
 free(largeItemset);

 free(numLarge);
 
 free(headerTableLink);

 destroyTree(root);

 return;
}



/******************************************************************************************
 * Function: swap
 *
 * Description:
 *	Swap x-th element and i-th element of each of the
 *	two arrays, support[] and itemset[].
 *
 * Invoked from:	
 *	q_sortD()
 *	q_sortA()
 * 
 * Functions to be invoked: None
 *
 * Input Parameters:
 *	support	-> Corresponding supports of the items in itemset.
 *	itemset	-> Array of items.
 *	x, i	-> The two indexes for swapping.
 *
 * Global variable: None
 */
void swap(int *support, int *itemset, int x, int i)
{ 
 int temp; 

 temp = support[x];
 support[x] = support[i];
 support[i] = temp;
 temp = itemset[x];
 itemset[x] = itemset[i];
 itemset[i] = temp;
 
 return;
}


/******************************************************************************************
 * Function: q_sortD
 *
 * Description:
 * 	Quick sort two arrays, support[] and the corresponding itemset[], 
 *	in descending order of support[].
 *
 * Invoked from:	
 *	pass1()
 *	genConditionalPatternTree()
 *	q_sortD()
 * 
 * Functions to be invoked:
 *	swap()
 *	q_sortD()
 *
 * Input Parameters:
 *      low		-> lower bound index of the array to be sorted
 *      high		-> upper bound index of the array to be sorted
 *      size		-> size of the array
 *	length		-> length of an itemset
 *
 * In/Out Parameters:
 *      support[]	-> array to be sorted
 *      itemset[]	-> array to be sorted
 */
void q_sortD(int *support, int *itemset, int low,int high, int size)
{
 int pass;
 int highptr=high++;     /* highptr records the last element */
 /* the first element in list is always served as the pivot */
 int pivot=low;

 if(low>=highptr) return;
 do {
	/* Find out, from the head of support[], 
	 * the 1st element value not larger than the pivot's 
	 */
	pass=1;
	while(pass==1) {
		if(++low<size) {
			if(support[low] > support[pivot])
				pass=1;
			else pass=0;
		} else pass=0;
	} 

	/* Find out, from the tail of support[], 
	 * the 1st element value not smaller than the pivot's 
	 */ 
	pass=1; 
	while(pass==1) {
		if(high-->0) { 
			if(support[high] < support[pivot]) 
				pass=1;
			else pass=0; 
		} else pass=0; 
	}

	/* swap elements pointed by low pointer & high pointer */
	if(low<high)
		swap(support, itemset, low, high);
 } while(low<=high);

 swap(support, itemset, pivot, high);

 /* divide list into two for further sorting */ 
 q_sortD(support, itemset, pivot, high-1, size); 
 q_sortD(support, itemset, high+1, highptr, size);
 
 return;
}


/******************************************************************************************
 * Function: q_sortA
 *
 * Description:
 * 	Quick sort two arrays, indexList[] and the corresponding freqItemP[], 
 *	in ascending order of indexList[].
 *
 * Invoked from:	
 *	buildTree()
 *	buildConTree()
 *	q_sortA()
 * 
 * Functions to be invoked:
 *	swap()
 *	q_sortA()
 *
 * Input Parameters:
 *      low		-> lower bound index of the array to be sorted
 *      high		-> upper bound index of the array to be sorted
 *      size		-> size of the array
 *	length		-> length of an itemset
 *
 * In/Out Parameters:
 *      indexList[]	-> array to be sorted
 *      freqItemP[]	-> array to be sorted
 */
void q_sortA(int *indexList, int *freqItemP, int low, int high, int size)
{
 int pass;
 int highptr=high++;     /* highptr records the last element */
 /* the first element in list is always served as the pivot */
 int pivot=low;

 if(low>=highptr) return;
 do {
        /* Find out, from the head of indexList[], 
	 * the 1st element value not smaller than the pivot's 
	 */
        pass=1;
        while(pass==1) {
                if(++low<size) {
                        if(indexList[low] < indexList[pivot])
                                pass=1;
                        else pass=0;
                } else pass=0;
        }

        /* Find out, from the tail of indexList[],
	 * 1st element value not larger than the pivot's 
	 */
        pass=1;
        while(pass==1) {
                if(high-->0) {
                        if(indexList[high] > indexList[pivot])
                                pass=1;
                        else pass=0;
                } else pass=0;
        }

        /* swap elements pointed by low pointer & high pointer */
        if(low<high)
                swap(indexList, freqItemP, low, high);
 } while(low<=high);

 swap(indexList, freqItemP, pivot, high);

 /* divide list into two for further sorting */
 q_sortA(indexList, freqItemP, pivot, high-1, size);
 q_sortA(indexList, freqItemP, high+1, highptr, size);

 return;
}

/******************************************************************************************
 * Function: insert_tree
 *
 * Description:
 *  	This function is to insert a frequent pattern
 *  	of a transaction to the FP-tree (or conditional FP-tree).
 *  	The frequent pattern consists of a list of the frequent 1-items
 *  	of a transaction, and is sorted according to the sorted order of the
 *  		1. frequent 1-items in function pass1(), if it is the initial FP-tree;
 *		2. frequent 1-items in the conditional pattern base, if it is a conditional FP-tree.
 *  	This function is recursively invoked and insert the (ptr+1)th item of the
 *  	frequent pattern in the (ptr+1)th round of recursion.
 *
 *	There are 3 cases to handle the insertion of an item:
 *	1. The tree or subtree being visited has no children.
 *		Create the first child and store the info of the item
 *		to this first child.
 *	2. The tree or subtree has no children that match the current item.
 *		Add a new child node to store the item.
 *	3. There is a match between the item and a child of the tree.
 *		Increment the count of the child, and visit the subtree of this child.
 *
 * Invoked from:	
 *	buildTree()
 *	buildConTree()
 *	insertTree()
 *
 * Functions to be invoked:
 *	insertTree()
 *
 * Parameters:
 *  - freqItemP : The list of frequent items of the transaction.
 *                They are sorted according to the order of frequent 1-items.
 *  - indexList : 'indexList[i]' is the corresponding index of the header table 
 *                that represents the item 'freqItemP[i]'.
 *  - count     : The initial value for the 'count' of the FP-tree node for the current
 *                freqItemP[i].
 *		  It is equal to 1 if the FP-tree is the initial one,
 *		  otherwiese it is equal to the support of the base of 
 *		  this conditional FP-tree.
 *  - ptr       : Number of items in the frequent pattern inserted so far.
 *  - length    : Number of items in the frequent pattern.
 *  - T         : The current FP-tree/subtree being visited so far.
 *  - headerTableLink : Header table of the FP-tree.
 *  - path      : Number of new tree path (i.e. new leaf nodes) created so far for the insertions.
 */
void insert_tree(int *freqItemP, int *indexList, int count, int ptr, int length, 
			FPTreeNode T, FPTreeNode *headerTableLink, int *path)  
{
 childLink newNode;
 FPTreeNode hNode;
 FPTreeNode hPrevious;
 childLink previous;
 childLink aNode;

 /* If all the items have been inserted */
 if (ptr == length) return;

 /* Case 1 : If the current subtree has no children */
 if (T->children == NULL) {
	/* T has no children */

	/* Create child nodes for this node */
	newNode = (childLink) malloc (sizeof(ChildNode));
	if (newNode == NULL) {
		printf("out of memory\n");
		exit(1);
	}

	/* Create a first child to store the item */
	newNode->node = (FPTreeNode) malloc (sizeof(FPNode));
	if (newNode->node == NULL) {
		printf("out of memory\n");
		exit(1);
	}

	/* Store information of the item */
	newNode->node->item = freqItemP[ptr];
	newNode->node->count = count;
	newNode->node->numPath = 1;
	newNode->node->parent = T;
	newNode->node->children = NULL;
	newNode->node->hlink = NULL;
	newNode->next = NULL;
	T->children = newNode;

	/* Link the node to the header table */
	hNode = headerTableLink[indexList[ptr]];
	if (hNode == NULL) {
		/* Place the node at the front of the horizontal link for the item */
		headerTableLink[indexList[ptr]] = newNode->node;
	} else {
		/* Place the node at the end using the horizontal link */
		while (hNode != NULL) {
			hPrevious = hNode;
			hNode = hNode->hlink;
		}

		hPrevious->hlink = newNode->node;
	}

	/* Insert next item, freqItemP[ptr+1], to the tree */
	insert_tree(freqItemP, indexList, count, ptr+1, length, T->children->node, headerTableLink, path);
	T->numPath += *path;

 } else {
	aNode = T->children;
	while ((aNode != NULL) && (aNode->node->item != freqItemP[ptr])) {
		previous = aNode;
		aNode = aNode->next;
	}

	if (aNode == NULL) {
		/* Case 2: Create a new child for T */ 

		newNode = (childLink) malloc (sizeof(ChildNode));
		if (newNode == NULL) {
			printf("out of memory\n");
			exit(1);
		}
		newNode->node = (FPTreeNode) malloc (sizeof(FPNode));
		if (newNode->node == NULL) {
			printf("out of memory\n");
			exit(1);
		}

		/* Store information of the item */
		newNode->node->item = freqItemP[ptr];
		newNode->node->count = count;
		newNode->node->numPath = 1;
		newNode->node->parent = T;
		newNode->node->children = NULL;
		newNode->node->hlink = NULL;
		newNode->next = NULL;
		previous->next = newNode;

		/* Link the node to the header table */
		hNode = headerTableLink[indexList[ptr]];
		if (hNode == NULL) {
			/* Place the node at the front of the horizontal link for the item */
			headerTableLink[indexList[ptr]] = newNode->node;
		} else {
			/* Place the node at the end using the horizontal link */
			while (hNode != NULL) {
				hPrevious = hNode;
				hNode = hNode->hlink;
			}
			hPrevious->hlink = newNode->node;
		}

		/* Insert next item, freqItemP[ptr+1], to the tree */
		insert_tree(freqItemP, indexList, count, ptr+1, length, newNode->node, headerTableLink, path);

		(*path)++;
		T->numPath += *path;

	} else {
		/* Case 3: Match an existing child of T */

		/* Increment the count */
		aNode->node->count += count;

		/* Insert next item, freqItemP[ptr+1], to the tree */
		insert_tree(freqItemP, indexList, count, ptr+1, length, aNode->node, headerTableLink, path);

		T->numPath += *path; 
	}
 }

 return;
}

/******************************************************************************************
 * Function: pass1()
 *
 * Description:
 *	Scan the DB and find the support of each item.
 *	Find the large 1-itemsets according to the support threshold.
 *
 * Invoked from:	
 *	main()
 *
 * Functions to be invoked:
 *	q_sortD()
 *
 * Global variables:
 *	largeItem1[]	-> Array to store 1-itemsets
 *	support1[]	-> Support[i] = support of the 1-itemset stored in largeItem[i]
 *	largeItemset[]	-> largeItemset[i] = resulting list for large (i+1)-itemsets
 *	realK		-> Maximum size of itemset to be mined
 *	numLarge[]	-> numLarge[i] = Number of large (i+1)-itemsets discovered so far
 *
 * Global variables (read only):
 *	numTrans	-> number of transactions in the database
 *	expectedK	-> User specified maximum size of itemset to be mined
 *	dataFile	-> Database file
 *	
 */
void pass1()
{
 int transSize;
 int item;
 int maxSize=0;
 FILE *fp;
 int i, j;

 /* Initialize the 1-itemsets list and support list */
 support1 = (int *) malloc (sizeof(int) * numItem);
 largeItem1 = (int *) malloc (sizeof(int) * numItem);
 if ((support1 == NULL) || (largeItem1 == NULL)) {
	printf("out of memory\n");
	exit(1);
 }

 for (i=0; i < numItem; i++) { 
	support1[i] = 0;
	largeItem1[i] = i;
 }

 /* scan DB to count the frequency of each item */

 if ((fp = fopen(dataFile, "r")) == NULL) {
        printf("Can't open data file, %s.\n", dataFile);
        exit(1);
 }

 /* Scan each transaction of the DB */
 for (i=0; i < numTrans; i++) {

	/* Read the transaction size */
	fscanf(fp, "%d", &transSize);

	/* Mark down the largest transaction size found so far */
	if (transSize > maxSize)
		maxSize = transSize;

	/* Read the items in the transaction */
	for (j=0; j < transSize; j++) {
		fscanf(fp, "%d", &item);
		support1[item]++;
	}
 } 
 fclose(fp);
 
 /* Determine the upper limit of itemset size to be mined according to DB and user input. 
  * If the user specified maximum itemset size (expectedK) is greater than 
  * the largest transaction size (maxSize) in the database, or  exptectedK <= 0,
  * then set  realK = maxSize;
  * otherwise  realK = expectedK
  */
 realK = expectedK;
 if ((maxSize < expectedK) || (expectedK <= 0))
	realK = maxSize;
 printf("max transaction sizes = %d\n", maxSize);
 printf("max itemset size (K_max) to be mined  = %d\n", realK);

 /* Initialize large k-itemset resulting list and corresponding support list */
 largeItemset = (LargeItemPtr *) malloc (sizeof(LargeItemPtr) * realK); 
 numLarge = (int *) malloc (sizeof(int) * realK);

 if ((largeItemset == NULL) || (numLarge == NULL)) {
	printf("out of memory\n");
	exit(1);
 }

 for (i=0; i < realK; i++)  {
	largeItemset[i] = NULL;
	numLarge[i] = 0;
 }

 /* Sort the supports of 1-itemsets in descending order */
 q_sortD(&(support1[0]), largeItem1, 0, numItem-1, numItem);

 /*
 for (i=0; i < numItem; i++) 
 	printf("%d[%d] ", largeItem1[i], support1[i]);
 printf("\n");
 */

 numLarge[0] = 0;
 while ((numLarge[0] < numItem) && (support1[numLarge[0]] >= threshold))
	(numLarge[0])++;

 printf("\nNo. of large 1-itemsets (numLarge[0]) = %d\n", numLarge[0]);

 return;
}


/******************************************************************************************
 * Function: buildTree()
 *
 * Description:
 *	Build the initial FP-tree.
 *
 * Invoked from:	
 *	main()
 *
 * Functions to be invoked:
 *	insert_tree()
 *	q_sortA()
 *
 * Global variables:
 *	root		-> Pointer to the root of this initial FP-tree
 *	headerTableLink	-> Header table for this initial FP-tree
 *
 * Global variables (read only):
 *	numLarge[]	-> Large k-itemsets resulting list for k = 1 to realK
 */
void buildTree()
{
 int *freqItemP;	/* Store frequent items of a transaction */
 int *indexList;	/* indexList[i] = the index position in the large 1-item list storing freqItemP[i] */
 int count;		/* Number of frequent items in a transaction */
 FILE *fp;		/* Pointer to the database file */
 int transSize;		/* Transaction size */
 int item;		/* An item in the transaction */
 int i, j, m;
 int path;		/* Number of new tree paths (i.e. new leaf nodes) created so far */


 /* Create header table */
 headerTableLink = (FPTreeNode *) malloc (sizeof(FPTreeNode) * numLarge[0]);
 if (headerTableLink == NULL) {
	printf("out of memory\n");
	exit(1);
 }
 for (i=0; i < numLarge[0]; i++)
	headerTableLink[i] = NULL;
	
 /* Create root of the FP-tree */
 root = (FPTreeNode) malloc (sizeof(FPNode));
 if (root == NULL) {
	printf("out of memory\n");
	exit(1);
 }

 /* Initialize the root node */
 root->numPath = 1;
 root->parent = NULL;
 root->children = NULL;
 root->hlink = NULL;

 /* Create freqItemP to store frequent items of a transaction */
 freqItemP = (int *) malloc (sizeof(int) * numItem);
 if (freqItemP == NULL) {
	printf("out of memory\n");
	exit(1);
 }	

 indexList = (int *) malloc (sizeof(int) * numItem);
 if (indexList == NULL) {
	printf("out of memory\n");
	exit(1);
 }	


 /* scan DB and insert frequent items into the FP-tree */
 if ((fp = fopen(dataFile, "r")) == NULL) {
        printf("Can't open data file, %s.\n", dataFile);
        exit(1);
 }


 for (i=0; i < numTrans; i++) {

	/* Read the transaction size */
	fscanf(fp, "%d", &transSize);

	count = 0;
 	path = 0;

	for (j=0; j < transSize; j++) {

		/* Read a transaction item */
		fscanf(fp, "%d", &item);

		/* Store the item to the frequent list, freqItemP, 
		 * if it is a large 1-item.
		 */
		for (m=0; m < numLarge[0]; m++) {
			if (item == largeItem1[m]) {
				/* Store the item */
				freqItemP[count] = item;
				/* Store the position in the large 1-itemset list storing this item */
				indexList[count] = m;
				count++;
				break;
			} 
		}
	}

	/* Sort the items in the frequent item list in ascending order of indexList,
	 * i.e. sort according to the order of the large 1-itemset list
	 */
	q_sortA(indexList, freqItemP, 0, count-1, count);

	/* Insert the frequent patterns of this transaction to the FP-tree. */
	insert_tree(&(freqItemP[0]), &(indexList[0]), 1, 0, count, root, headerTableLink, &path);
 } 
 fclose(fp);

 free(freqItemP);
 free(indexList);
 free(largeItem1);
 free(support1);

 return;
}


/******************************************************************************************
 * Function: input
 *
 * Description:
 *	Read the input parameters from the configuration file.
 *
 * Invoked from:	
 *	main()
 *
 * Functions to be invoked: None
 *
 * Input parameters:
 *	*configFile	-> The configuration file
 *
 * Global variables:
 *	expectedK		-> User specified maximum size of itemset to be mined
 *	thresholdDecimal	-> Normalized support threshold, range: (0, 1]
 *	numItem			-> Total number of different items in the DB
 *	numTrans		-> Total number of transactions in the DB
 *	dataFile		-> Data file
 *	outFile			-> Result file for storing the large itemsets
 */
void input(char *configFile)
{
 FILE *fp;
 float thresholdDecimal;

 if ((fp = fopen(configFile, "r")) == NULL) {
        printf("Can't open config. file, %s.\n", configFile);
        exit(1);
 }

 fscanf(fp, "%d %f %d %d", &expectedK, &thresholdDecimal, &numItem, &numTrans);
 fscanf(fp, "%s %s", dataFile, outFile);
 fclose(fp);

 printf("expectedK = %d\n", expectedK);
 printf("thresholdDecimal = %f\n", thresholdDecimal);
 printf("numItem = %d\n", numItem);
 printf("numTrans = %d\n", numTrans);
 printf("dataFile = %s\n", dataFile);
 printf("outFile = %s\n\n", outFile);
 threshold = thresholdDecimal * numTrans;
 if (threshold == 0) threshold = 1;
 printf("threshold = %d\n", threshold);

 return;
}
/******************************************************************************************
 *Function: show_time
 *
 *Description:
 *	will be used later
 *
 */
void show_time(){
	int time1=clock()/CLOCKS_PER_SEC;
	printf("time1: %u secs.\n", time1);
}

/******************************************************************************************
 *Function: test_tree()
 *
 *Description:
 *	
 */
void test_tree(FPTreeNode snode, vector<string> & (* f)(int* p, int index, vector<string> & v)){ // this is to find all the branches based on leaf-nodes
	FPTreeNode pnode = snode;
	vector<string> & (*combine_string)(int* p, int index, vector<string> & v) = f;
	if(pnode->children){
			//printf("%d\n", pnode->children->node->item);
			//test_tree(pnode->children->node); //this is depth first
		while(pnode->children){ //this is also depth first
			test_tree(pnode->children->node, combine_string);
			pnode->children = pnode->children->next;
		}
	}
	else{
		total_leaf_node++;
		printf("this is leaf: %d, count: %d\n", pnode->item, pnode->count);
		branch_i++;
		branch_j = 0;

		///////////////////
		vector<string> v_combine;
		int length_of_branch = 0; 
		FPTreeNode tmp = pnode;
		while(tmp!=root)//get the length of each branch
		{
			length_of_branch++;
			tmp = tmp->parent;
		}
		int * pp = (int *)malloc(sizeof(int)*length_of_branch);//int array store items in one branch
		int loop = length_of_branch;
		while(pnode!=root)
		{
			pp[--length_of_branch] = pnode->item;
			pnode = pnode->parent;
		}
		v_combine = combine_string(pp, loop-1, v_combine);
		for(vector<string>::iterator jj = v_combine.begin();jj<v_combine.end();jj++)
		{
			cout<<"vector: "+ *jj<<endl;
		}
	}
}
/******************************************************************************************
 *Function: combine_string(FPTreeNode pnode)
 *
 *Description:
 * combination starts from leaf node.
 */
vector<string> & combine_string(int * p, int index, vector<string> & v)
{
	if(index == 0)
	{	
			/////this is for convert int to string
		int i = p[index];
		std::string s;
		std::stringstream out;
		out << i;
		s = out.str();
			/////
		v.push_back(s);
		return v;
	}
	else
	{
		v = combine_string(p, index-1, v);
			///////this is for convert int to string
		int i = p[index];
		std::string s;
		std::stringstream out;
		out << i;
		s = out.str();
		////////
		for(int n=0;n<2*index-1;n++)
		{
			v.push_back(v.at(n)+" "+s);
		}
		v.push_back(s);
		return v;
	}
}

/******************************************************************
*/
char ** read_file_and_display(int a, int b)
{
	printf("numTrans: %d, numItem: %d\n", a, b);
	//////-------------------------------------    pp is 2-D array
	char ** pp = (char **)malloc(sizeof(char) * a); 
	if(pp == NULL){
		printf("not enough space\n");
		exit(0);
	}
	for(int i = 0;i< a;i++){
		pp[i] = (char *)malloc(sizeof(char) * b);
		if(pp == NULL)
			printf("not enough space\n");
	}

	for(int k=0;k<a;k++){
		memset(pp[k], 0 , b);
	}
	///////////////////////this is generate for and array
	//int * branch = (int *)malloc(sizeof(int)*numItem);
	//memset(branch, 0, numItem);
	/////////////////////////////////////////////////////
	char line[200];
	int i=0;
	int j;
	
	FILE * fp = fopen(dataFile, "r");
	if(fp == NULL)
	{
		printf("can't open the file\n");
	}
	else
	{
		while(fgets(line, 200, fp) != NULL)
		{
			//puts(line);
			char * p = strtok(line, " ");
			p = strtok (NULL, " ");
			while (p != NULL)
			{
				//j = *p-'0'; //only for single number, not for double, such as 12, 23 etc.
				j = atoi(p);
				//printf ("%d\n",j);
				p = strtok (NULL, " ");
				pp[i][j] = 1;
			}
			i++;
		}
	}
	fclose(fp);
	return pp;
}
/******************************************************************************************
 * Function: main
 *
 * Description:
 *	This function reads the config. file for six input parameters,
 *	finds the frequent 1-itemsets, builds the initial FP-tree 
 *	using the frequent 1-itemsets and 
 *	peforms the FP-growth algorithm of the paper.
 *	It measure both CPU and I/O time for build tree and mining.
 *
 * Functions to be invoked: 
 *	input()		-> Read config. file
 *	pass1()		-> Scan DB and find frquent 1-itemsets
 *	buildTree()	-> Build the initial FP-tree
 *	FPgrowth()	-> Start mining
 *	
 * Parameters:
 *	Config. file name
 */
void main(int argc, char *argv[])
{
 //float time1, time2, time3;
 int headerTableSize;

 /* Usage ------------------------------------------*/
 printf("\nFP-tree: Mining large itemsets using user support threshold\n\n");
 if (argc != 2) {
        printf("Usage: %s <config. file>\n\n", argv[0]);
	printf("Content of config. file:\n");
	printf("  Line 1: Upper limit of large itemsets size to be mined\n");
	printf("  Line 2: Support threshold (normalized to [0, 1])\n");
	printf("  Line 3: No. of different items in the DB\n");
	printf("  Line 4: No. of transactions in the DB\n");
	printf("  Line 5: File name of the DB\n");
	printf("  Line 6: Result file name to store the large itemsets\n\n");
        exit(1);
 }

 /* read input parameters --------------------------*/
 printf("input\n");
 input(argv[1]);
 //char ** and_array = read_file_and_display(numTrans, numItem);//a branch array 
 
 /* pass 1 : Mine the large 1-itemsets -------------*/
 printf("\npass1\n");
 pass1();
 /* Mine the large k-itemsets (k = 2 to realK) -----*/
 if (numLarge[0] > 0) {
	/* create FP-tree --------------------------*/
 	printf("\nbuildTree\n");
 	buildTree();

	/*iterate through the branches:*/
	test_tree(root, combine_string);

	printf("total leaf number: %d", total_leaf_node);
	
	
	/* mining frequent patterns ----------------*/
 	printf("\npassK\n");
	headerTableSize = numLarge[0];
	numLarge[0] = 0;
 	//FPgrowth(root, headerTableLink, headerTableSize, NULL, 0);


 	/* output result of large itemsets ---------*/
 	printf("\nresult\n");
 	//displayResult();
 }

 /* free memory ------------------------------------*/
 printf("\ndestroy\n");
 destroy();

 return;
}


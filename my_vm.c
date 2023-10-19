/*
	Yashraj Nikam (yan7)
	Dhruv Kedia (dk1157)

ilab machine: kill.cs.rutgers.edu
*/
#include "my_vm.h"
#include <stdatomic.h>
struct physicalMemory physMem = {NULL};
unsigned pageTableIndexLen;
unsigned pageDirIndexLen;
char* vBitMap;
void* pgdir;
int lookups = 0;
int misses = 0;
volatile atomic_flag flag;
 FILE *fptr;
unsigned long long mylog2(unsigned long long num)
{
	int count = 0;
	while(num!=1)
	{
		num=num>>1;
		count++;
	}
	return count;
}
int max(int a,int b)
{
	return a<b?b:a;
}
static void set_bit_at_index(char *bitmap, int index)
{
	bitmap[index/8] |= 1 << index%8;
   	return;
}
static void reset_bit_at_index(char *bitmap, int index)
{
	bitmap[index/8] &= 0 << index%8;
   	return;
}
static int get_bit_at_index(char *bitmap, int index)
{
    return bitmap[index/8] >> index%8 & 1;
}
static unsigned int get_top_bits(unsigned int value,  int num_bits)
{
	return value>>sizeof(value)*8-num_bits;	
}
int calculateLevels()
{
	int ptes = mylog2(PGSIZE/sizeof(pte_t));
	int vpn = mylog2(MAX_MEMSIZE/PGSIZE);
	return (int)ceil((double)vpn/ptes);
}
void* set_physical_mem() {

	 fptr = fopen("./op.txt", "w");
    if (fptr == NULL)
    {
        printf("Could not open file");
        return 0;
    }
	physMem.basePtr = malloc(MEMSIZE);
	physMem.numOfFrames = MEMSIZE/PGSIZE;
	physMem.bitMap = (char*)calloc(physMem.numOfFrames/(sizeof(char)*8),sizeof(char));
	
	int levels = calculateLevels();
	pageDirIndexLen = mylog2(MAX_MEMSIZE/PGSIZE) - mylog2(PGSIZE/sizeof(pte_t))*(levels-1);
	int pagesAtEachLevel = MAX_MEMSIZE / PGSIZE * sizeof(pte_t) / PGSIZE;
	for(int i=0;i<pagesAtEachLevel;i++)
		set_bit_at_index(physMem.bitMap,i);
	vBitMap = (char*)calloc(MAX_MEMSIZE/PGSIZE/(sizeof(char)*8),sizeof(char));
	
	pte_t* base = physMem.basePtr;
	int entries = PGSIZE/sizeof(pte_t);
	base+=pagesAtEachLevel*entries;
	int prevPages = 0;
	int index = 0;
	for(int i=0;i<levels-1;i++)
	{
		prevPages += pagesAtEachLevel;
		pagesAtEachLevel = (int)ceil((double)pagesAtEachLevel * sizeof(pte_t) / PGSIZE);
		for(int j=0;j<pagesAtEachLevel;j++)
			set_bit_at_index(physMem.bitMap,prevPages+j);
		for(int k=0;k<prevPages;k++)
			base[k] = index+k;
		base+=pagesAtEachLevel*entries;
		index+=prevPages;
	}
	return base-pagesAtEachLevel*entries;
}

int add_TLB(void *va, void *pa)
{
	printf("Added %u as %u\n",(unsigned)va,(unsigned)pa);
	int index = (int)va>>mylog2(PGSIZE);
	if(tlb_store.va_arr[index%TLB_ENTRIES]==index && tlb_store.pa_arr[index])
		return 0;
	else
	{
		tlb_store.va_arr[index%TLB_ENTRIES]=index;
		tlb_store.pa_arr[index]=pa;
	}
    	return -1;
}
pte_t * check_TLB(void *va) {

	int index = (int)va>>mylog2(PGSIZE);
	if(tlb_store.va_arr[index%TLB_ENTRIES]==index && tlb_store.pa_arr[index])
		return tlb_store.pa_arr[index];
	return NULL;
}
void print_TLB_missrate()
{
	double miss_rate = 0;	
	miss_rate = (double)misses / lookups;
    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}
pte_t *translate(pde_t *pgdir, void *va) {
    	pte_t* check;
    	lookups++;
    	if(check = check_TLB(va))
    	{
    		unsigned off = (unsigned)get_top_bits(((unsigned)va<<(mylog2(MAX_MEMSIZE) - mylog2(PGSIZE))),mylog2(PGSIZE));
    		fprintf(fptr,"Offset for %u is %u\n",(unsigned)va,off);
    		return check+off/sizeof(pte_t);
    	}
    	misses++;
    	int offsetAtEachLevel = pageDirIndexLen;
    	int shift = 0;
    	int levels = calculateLevels();
    	pte_t* base = pgdir;  
	for(int i=0;i<levels ;i++)
   	{
		base += get_top_bits((int)va<<shift,offsetAtEachLevel);
		base = *base * (PGSIZE/sizeof(pte_t))+(pte_t*)physMem.basePtr;
		shift += offsetAtEachLevel;
		offsetAtEachLevel = (mylog2(MAX_MEMSIZE/PGSIZE) - pageDirIndexLen)/(levels-1);
    	}
    	add_TLB(va,base);
	pte_t* a = base;
    	return a+(unsigned)get_top_bits(((int)va<<(mylog2(MAX_MEMSIZE) - mylog2(PGSIZE))),mylog2(PGSIZE));
}

int get_next_avail(int num_pages) {					//changed return type from void* to int
 
    	int count = 0;
    	for(int i=0;i<MAX_MEMSIZE/PGSIZE;i++)
    	{
    		if(count==num_pages)
    			return i-count;							
    		if(!get_bit_at_index(vBitMap,i))
    			count++;
    		else
    			count = 0;
    	}	
    	return -1;
}
void *t_malloc(unsigned int num_bytes) {
	while(atomic_flag_test_and_set(&flag));
	
	if(!physMem.basePtr)
		pgdir = set_physical_mem();

    	int num_pages = (int)ceil((double)num_bytes / PGSIZE);
    	unsigned long freeInd = get_next_avail(num_pages);
	if(freeInd==-1)
    		return NULL;
    	int* freeFrameNum = (int*)malloc(num_pages*sizeof(int));
    	int ind=0;
    	for(int i=0;i<physMem.numOfFrames;i++)
    	{
    		if(ind == num_pages)
    			break;
    		if(!get_bit_at_index(physMem.bitMap,i))
    			freeFrameNum[ind++] = i;
    	}
    	if(ind!=num_pages)
    		return NULL;
    	pte_t* base = physMem.basePtr;
    	int entries = PGSIZE/sizeof(pte_t);
    	for(int i=0;i<num_pages;i++)
    	{
    		set_bit_at_index(vBitMap,freeInd+i);
    		base[freeInd+i] = freeFrameNum[i];
    		set_bit_at_index(physMem.bitMap,freeFrameNum[i]);
    	}
		atomic_flag_clear(&flag);
    	return (unsigned long*)(freeInd<<mylog2(PGSIZE));
}
void t_free(void *va, int size) {
	 int num_pages = (int)ceil((double)size / PGSIZE);
     	if(!num_pages)
    		num_pages++;
    	unsigned long index = (unsigned long)va>>mylog2(PGSIZE);
    	for(int i=0;i<num_pages;i++)
    		if(!vBitMap[index+i])
    			return;
    	for(int i=0;i<num_pages;i++)
    	{
    		reset_bit_at_index(physMem.bitMap,((pte_t*)physMem.basePtr)[index+i]);
    		reset_bit_at_index(vBitMap,index+i);
    	}
     
	int ind = (int)va>>mylog2(PGSIZE);
	if(tlb_store.va_arr[ind%TLB_ENTRIES]==ind && tlb_store.pa_arr[ind])
	{
		tlb_store.va_arr[ind] = -1;
		tlb_store.pa_arr[ind] = NULL;
	}
}
int put_value(void *va, void *val, int size) 
{
	while(atomic_flag_test_and_set(&flag));
	int num_pages = (int)ceil((double)size / PGSIZE);
	unsigned long index = (unsigned long)va>>mylog2(PGSIZE);
	for(int i=0;i<num_pages;i++)
		if(!get_bit_at_index(vBitMap,index+i))
    			return -1;
	for(int i=0;i<num_pages;i++)
	{
		unsigned* base = (unsigned*)translate(pgdir,(void*)((int*)va+i));
		fprintf(fptr,"PutValue %u->%u\n",(unsigned)va,(unsigned)base);
		*base = *((unsigned*)val + i);
	}
	atomic_flag_clear(&flag);
	return 0;
}
void get_value(void *va, void *val, int size) 
{
	while(atomic_flag_test_and_set(&flag));
	int num_pages = (int)ceil((double)size / PGSIZE);
	unsigned long index = (unsigned long)va>>mylog2(PGSIZE);
	for(int i=0;i<num_pages;i++)
		if(!get_bit_at_index(vBitMap,index+i))
    			return;

	for(int i=0;i<num_pages;i++)
	{
		unsigned* base =(unsigned*) translate(pgdir,(void*)((int*)va+i));
		fprintf(fptr,"GetValue %u->%u\n",(unsigned)va,(unsigned)base);
		*((unsigned*)val+i) = *base;
	}
	atomic_flag_clear(&flag);
}
void mat_mult(void *mat1, void *mat2, int size, void *answer) {
    int x, y, val_size = sizeof(int);
    int i, j, k;
    //printf("Mat1: %x\n",(int)mat1);
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
               int address_a = (unsigned int)mat1 + ((i* size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));                
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                c += (a * b);
                fprintf(fptr,"%u:%u %u:%u \n",address_a,a,address_b,b);
            //    printf("%d %d %d\n",a,b,c);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            fprintf(fptr,"%u:%u\n",address_c,c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}

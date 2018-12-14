struct region {
	  struct region *next;
	    uint32_t base;
	      uint32_t size;
	        int  (*init)(struct region *r);
		  int  (*set)(struct region *r, uint32_t address, uint8_t mask, uint32_t value);
		    int  (*get)(struct region *r, uint32_t address, uint32_t *value);
		      void (*free)(struct region *r);
		        void (*dump)(struct region *r);
			  void *data;
};

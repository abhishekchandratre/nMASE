int isLiteral(WORDSIZE word)
{
	if(word>0 && ((word & 1<<(BITS_IN_WORD-1)) == 0))  //if (word is not 0) and (its first bit is not 0)
		return 1;
	return 0;
}

int isFill(WORDSIZE word)
{
	if(word == 0)
		return 1;
	else if((word & 1<<(BITS_IN_WORD-1)) > 0)	//if word has its first bit set
		return 1;
	return 0;
}

WORDSIZE little_to_big(WORDSIZE num)			//WORKS ONLY FOR 32 bit WORD SIZE!
{			
	WORDSIZE swapped = ((num>>24)&0xff) | // move byte 3 to byte 0
                    ((num<<8)&0xff0000) | // move byte 1 to byte 2
                    ((num>>8)&0xff00) | // move byte 2 to byte 1
                    ((num<<24)&0xff000000); // byte 0 to byte 3
	return swapped;
}

void add_IP_bitmap(IP_bitmap *ipbm, u_int8_t IP [], int flowID)
{
	unsigned short charno, bitpos;

	charno = flowID/8;				//Finding the character number and bit position to set
	if((bitpos = flowID % 8) == 0)			//handling edge case where flow ID is a multiple of 8
	{
		charno--;
		bitpos = 8;
	}
	ipbm->octet1[IP[0]][charno] |= 1<<(8-bitpos);	//Set corresponding bits in all 4 octets, for that particular flow ID
	ipbm->octet2[IP[1]][charno] |= 1<<(8-bitpos);
	ipbm->octet3[IP[2]][charno] |= 1<<(8-bitpos);
	ipbm->octet4[IP[3]][charno] |= 1<<(8-bitpos);
	
	/*
	printf("\nOctet 1 for value : %d charno : %d bit position : %d byte : %" PRId8,IP[0],charno,bitpos,ipbm->octet1[IP[0]][charno]); 
	printf("\nOctet 2 for value : %d charno : %d bit position : %d byte : %" PRId8,IP[1],charno,bitpos,ipbm->octet2[IP[1]][charno]); 
	printf("\nOctet 3 for value : %d charno : %d bit position : %d byte : %" PRId8,IP[2],charno,bitpos,ipbm->octet3[IP[2]][charno]); 
	printf("\nOctet 4 for value : %d charno : %d bit position : %d byte : %" PRId8,IP[3],charno,bitpos,ipbm->octet4[IP[3]][charno]);
	getchar(); */
	
}

void add_port_bitmap(port_bitmap *prbm, u_int16_t portno, int flowID)
{
	unsigned short charno, bitpos;

	charno = flowID / 8;			//Finding the character number and bit position to set
	if((bitpos = flowID % 8) == 0)		//handling edge case where flow ID is a multiple of 8
	{
		charno--;
		bitpos = 8;
	}
	prbm->port[portno][charno] |= 1<<(8-bitpos); //set corresponding bit in bitmap, for that particular flow ID
	//printf("\nFor port %d, char no : %d bit position : %d byte : %" PRId8,portno,charno,bitpos,prbm->port[portno][charno]); 
}

void add_tproto_bitmap(tproto_bitmap *tpbm, u_int8_t tprotonum, int flowID)
{
	unsigned short charno, bitpos;

	charno = flowID / 8;			//Finding the character number and bit position to set
	if((bitpos = flowID % 8) == 0)		//handling edge case where flow ID is a multiple of 8
	{
		charno--;
		bitpos = 8;
	}
	tpbm->tproto[tprotonum][charno] |= 1<<(8-bitpos);
	//printf("\nFor protocol %d, char no : %d bit position : %d byte : %" PRId8,tprotonum,charno,bitpos,tpbm->tproto[tprotonum][charno]); 
}


int compress_bitvector(u_char *dstbitvector, u_char *srcbitvector)
{
	int i,j,dcharno=0,dbitpos=7,scharno=0,sbitpos=7;
	WORDSIZE *curWord = (WORDSIZE *)dstbitvector;
	WORDSIZE *newWord = (WORDSIZE *)calloc(1,NO_FLOWS);
	int noWords;
	
	//Make groups of 31 bits
	for(i=1; i<= NO_FLOWS*8; i++)
	{
		if((dcharno % BYTES_IN_WORD == 0) && dbitpos==7)//first bit of each 32 bit word must be 0
		{						//handles this case
			dstbitvector[dcharno] = 0;
			dbitpos--;
			continue;
		}
		
		if((srcbitvector[scharno] & 1<<sbitpos) != 0)
		{
			dstbitvector[dcharno] |= 1<<dbitpos;
		}
		
		sbitpos--;	
		dbitpos--;
		
		if(sbitpos==-1)
		{
			scharno++;
			sbitpos=7;
		}

		if(dbitpos == -1)
		{
			dcharno++;
			dbitpos=7;
		}

	}
	
	//Copy to another array, merging if required
	noWords = NO_FLOWS/BYTES_IN_WORD;
	for(i=0,j=0; i<noWords; i++)
	{
		curWord[i] = little_to_big(curWord[i]);		//Fixed line
		if(isLiteral(curWord[i]))			//If word is literal, write as it is
		{
			newWord[j] = curWord[i];				//Fixed line
			j++;
		}
		else if((i==0 || isLiteral(newWord[j-1])) && isFill(curWord[i]))
		{						//If word is fill, and previous word is literal,
			newWord[j] = curWord[i];		//write as it is, setting first bit to 1
			newWord[j] |= 1<<(BITS_IN_WORD-1);
			newWord[j]++;
			j++;
		}
		else if(isFill(newWord[j-1]) && isFill(curWord[i]))	//If word is fill and previous was also fill,
		{							//Increment the counter in previous word
			newWord[j-1]++;
		}
		
	}
	
	memcpy(dstbitvector,newWord,NO_FLOWS);			//Copy to the output argument
	free(newWord);
	
	//printf("\nCompressed bitvector :");
	//for(i=0;i<j;i++)
		//printf("\n%d:%" PRIu32,i,newWord[i]);
	
	return j;
}	

void compress_IP_bitmap(IP_bitmap *ipbm, const char *filename)
{
	int i,j,currLoc=0;
	IP_bitmap *newipbm = (IP_bitmap *)calloc(1,sizeof(IP_bitmap));
	WORDSIZE *curWord;
	u_int32_t startLoc[4][256];	
	int fp;
	
	if((fp = creat(filename, S_IRWXU | S_IRWXG | S_IROTH)) == -1)
	{
		printf("\nError opening file. %s",filename);
		return;
	}
	
	lseek(fp,sizeof(startLoc),SEEK_SET);	//Leave space to store starting locations of 256x4 bit vectors

	//compress and store octet 1
	for(i=0; i<=255; i++)
	{
		startLoc[0][i] = currLoc;	//Record its starting location (byte in the file)
		j = compress_bitvector(newipbm->octet1[i],ipbm->octet1[i]);	//compress the bitmap, noting down the compressed size
		currLoc += j*BYTES_IN_WORD;		//Increment starting location counter for the next bitmap

		//curWord = (WORDSIZE *)newipbm->octet1[192];
		//for(i=0;i<j;i++)
		//	printf(" %" PRIu32 " : ",curWord[i]);
		//printf("\n\n");
		
		write(fp,newipbm->octet1[i],j*BYTES_IN_WORD);	//Write the compressed bitmap to the file
	}
		
	//compress and store octet 2
	for(i=0; i<=255; i++)
	{
		startLoc[1][i] = currLoc;
		j = compress_bitvector(newipbm->octet2[i],ipbm->octet2[i]);
		currLoc += j*BYTES_IN_WORD;		
		
		write(fp,newipbm->octet2[i],j*BYTES_IN_WORD);	
	}

	//compress and store octet 3
	for(i=0; i<=255; i++)
	{
		startLoc[2][i] = currLoc;
		j = compress_bitvector(newipbm->octet3[i],ipbm->octet3[i]);
		currLoc += j*BYTES_IN_WORD;		
		
		write(fp,newipbm->octet3[i],j*BYTES_IN_WORD);	
	}

	//compress and store octet 4
	for(i=0; i<=255; i++)
	{
		startLoc[3][i] = currLoc;
		j = compress_bitvector(newipbm->octet4[i],ipbm->octet4[i]);
		currLoc += j*BYTES_IN_WORD;		
		
		write(fp,newipbm->octet4[i],j*BYTES_IN_WORD);
	}
	
	lseek(fp,0,SEEK_SET);				//jump back to the beginning of file
	write(fp,startLoc,sizeof(startLoc));		//write starting locations of 256x4 bitmaps (this is equivalent of header)
	printf("\ncompr IP bitmap written to file %s.",filename);
	
	//for(i=0; i<4; i++)
	//{
	//	for(j=0; j<=255; j++)
	//	{
	//		printf(" %d:%" PRIu32,j,startLoc[i][j]);
	//	}
	//	printf("\n");
	//}

	free(newipbm);
	close(fp);
}

void compress_port_bitmap(port_bitmap *prbm, const char *filename)
{
	int i,j,currLoc=0;
	port_bitmap *newprbm = (port_bitmap *)calloc(1,sizeof(port_bitmap));
	WORDSIZE *curWord;
	u_int32_t startLoc[65536];	
	int fp;
	
	if((fp = creat(filename, S_IRWXU | S_IRWXG | S_IROTH)) == -1)
	{
		printf("\nError opening file. %s",filename);
		return;
	}
	
	lseek(fp,sizeof(startLoc),SEEK_SET);	//Leave space to store starting locations of 256x4 bit vectors

	for(i=0; i<=65535; i++)			//for 65k bit vectors,
	{
		startLoc[i] = currLoc;		//store their starting location (byte offset)
		j = compress_bitvector(newprbm->port[i],prbm->port[i]);	//compress them, noting down the compressed size
		currLoc += j*BYTES_IN_WORD;		//increment starting location pointer accordingly
		
		write(fp,newprbm->port[i],j*BYTES_IN_WORD);	//Write down the compressed bitmap
	}

	//for(j=0; j<=100; j++)
	//{
	//		printf(" %d: %" PRIu32,j,startLoc[j]);
	//}
	
	lseek(fp,0,SEEK_SET);			//jump back to the beginning
	write(fp,startLoc,sizeof(startLoc));	//write starting locations of all 65k bit vectors
	printf("\ncompr port bitmap written to file %s.",filename);
	
	close(fp);
	free(newprbm);

}

void compress_tproto_bitmap(tproto_bitmap *tpbm, const char *filename)
{
	int i,j,currLoc=0;
	tproto_bitmap *newtpbm = (tproto_bitmap *)calloc(1,sizeof(tproto_bitmap));
	WORDSIZE *curWord;
	u_int32_t startLoc[3];	
	int fp;
	
	if((fp = creat(filename, S_IRWXU | S_IRWXG | S_IROTH)) == -1)
	{
		printf("\nError opening file. %s",filename);
		return;
	}
	
	lseek(fp,sizeof(startLoc),SEEK_SET);	//Leave some space to write starting locations of 3 bit vectors

	for(i=0; i<3; i++)
	{
		startLoc[i] = currLoc;		//Store the starting location of bit vector
		j = compress_bitvector(newtpbm->tproto[i],tpbm->tproto[i]);	//Compress that bit vector, noting down compressed size
		currLoc += j*BYTES_IN_WORD;		//Increment starting location pointer accordingly
		
		write(fp,newtpbm->tproto[i],j*BYTES_IN_WORD);	//write bit vector to the file
	}

	//for(j=0; j<3; j++)
	//{
	//		printf(" %d: %" PRIu32,j,startLoc[j]);
	//}
	
	lseek(fp,0,SEEK_SET);		//Jump back to the beginning of file
	write(fp,startLoc,sizeof(startLoc));//Write starting locations of 3 bit vectors
	printf("\ncompr tproto bitmap written to file %s.",filename);
	
	close(fp);
	free(newtpbm);

}


void save_IP_bitmap(IP_bitmap *ipbm, char *filename)
{
	int fp;
	
	if((fp = creat(filename, S_IRWXU | S_IRWXG | S_IROTH)) == -1)
	{
		printf("\nError opening file. %s",filename);
		return;
	}

	write(fp,ipbm,sizeof(IP_bitmap));		//Write uncompressed bitmap as it is to the file
	printf("\nuncompr IP bitmap written to file %s.",filename);
	
	close(fp);
}

void save_port_bitmap(port_bitmap *prbm, char *filename)
{
	int fp;
	
	if((fp = creat(filename, S_IRWXU | S_IRWXG | S_IROTH)) == -1)
	{
		printf("\nError opening file. %s",filename);
		return;
	}

	write(fp,prbm,sizeof(port_bitmap));		//Write uncompressed port bitmap as it is to the file
	printf("\nuncompr port bitmap written to file %s.",filename);
	
	close(fp);
}

void save_tproto_bitmap(tproto_bitmap *tpbm, char *filename)
{
	int fp;

	if((fp = creat(filename, S_IRWXU | S_IRWXG | S_IROTH)) == -1)
	{
		printf("\nError opening file. %s",filename);
		return;
	}

	write(fp,tpbm,sizeof(tproto_bitmap));		//Write uncompressed transport protocol bitmap as it is to the file
	printf("\nuncompr tproto bitmap written to file %s.",filename);
	
	close(fp);
}

void display_IP_bitmap(IP_bitmap *ipbm, int totalFlow)
{
	int i,j,k;
	
	totalFlow = totalFlow/8 + 1;
	if(totalFlow%8 == 0)
		totalFlow--;
	
	//Display 255 bit vectors of octet 1
	printf("\nOctet 1 :- \n");
	for(i=1; i<=255; i++)
	{
		printf("%03d:",i);
		for(j=0; j<totalFlow; j++)
		{
			for(k=7; k>=0; k--)
			{
				if( (ipbm->octet1[i][j] & 1<<k) > 0)
					printf("1");
				else
					printf("0");
			}
		}
		printf("\n");
	}
	getchar();
	
	//Display 255 bit vectors of octet 2
	printf("\nOctet 2 :- \n");
	for(i=1; i<=255; i++)
	{
		printf("%03d:",i);
		for(j=0; j<totalFlow; j++)
		{
			for(k=7; k>=0; k--)
			{
				if( (ipbm->octet2[i][j] & 1<<k) > 0)
					printf("1");
				else
					printf("0");
			}
		}
		printf("\n");
	}
	getchar();
	
	//Display 255 bit vectors of octet 3
	printf("\nOctet 3 :- \n");
	for(i=1; i<=255; i++)
	{
		printf("%03d:",i);
		for(j=0; j<totalFlow; j++)
		{
			for(k=7; k>=0; k--)
			{
				if( (ipbm->octet3[i][j] & 1<<k) > 0)
					printf("1");
				else
					printf("0");
			}
		}
		printf("\n");
	}
	getchar();
	
	//Display 255 bit vectors of octet 1
	printf("\nOctet 4 :- \n");
	for(i=1; i<=255; i++)
	{
		printf("%03d:",i);
		for(j=0; j<totalFlow; j++)
		{
			for(k=7; k>=0; k--)
			{
				if( (ipbm->octet4[i][j] & 1<<k) > 0)
					printf("1");
				else
					printf("0");
			}
		}
		printf("\n");
	}
	getchar();
	
}

void display_port_bitmap(port_bitmap *prbm, int totalFlow)
{
	int i,j,k;

	totalFlow = totalFlow/8 + 1;
	if(totalFlow%8 == 0)
		totalFlow--;
	
	//display 65k bit vectors of port bitmap
	printf("\nPort number :- \n");
	for(i=1; i<=65535; i++)
	{
		printf("%05d:",i);
		for(j=0; j<totalFlow; j++)
		{
			for(k=7; k>=0; k--)
			{
				if( (prbm->port[i][j] & 1<<k) > 0)
					printf("1");
				else
					printf("0");
			}
		}
		printf("\n");
		if(i%100 == 0)
			getchar();
	}
	
}

void display_tproto_bitmap(tproto_bitmap *tpbm, int totalFlow)
{
	int i,j,k;

	totalFlow = totalFlow/8 + 1;
	if(totalFlow%8 == 0)
		totalFlow--;
	
	//display 3 bit vectors of transport protocol bitmap	
	printf("\nTransport protocol :- \n");
	for(i=0; i<3; i++)
	{
		printf("%d:",i);
		for(j=0; j<totalFlow; j++)
		{
			for(k=7; k>=0; k--)
			{
				if( (tpbm->tproto[i][j] & 1<<k) > 0)
					printf("1");
				else
					printf("0");
			}
		}
		printf("\n");
	}
	
}

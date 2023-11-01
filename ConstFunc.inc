//-,!,~
/*
ConstFunc(ETN_OP_NEG,CV_CHAR)
{
	pA->nData=-pA->nData;
	return true;
}
ConstFunc(ETN_OP_NEG,CV_SHORT)
{
	pA->nData=-pA->nData;
	return true;
}
ConstFunc(ETN_OP_NEG,CV_INT)
{
	pA->nData=-pA->nData;
	return true;
}
ConstFunc(ETN_OP_NEG,CV_FLOAT)
{
	*((float *)&pA->nData)=-*((float *)&pA->nData);
	return true;
}
ConstFunc(ETN_OP_NEG,CV_DOUBLE)
{
	*((double *)&pA->nData)=-*((double *)&pA->nData);
	return true;
}
*/

ConstFunc(ETN_OP_NOT,CV_NULL)
{
	pA->nData=1;
	pA->uType=CV_INT;
	return true;
}
ConstFunc(ETN_OP_NOT,CV_CHAR)
{
	pA->nData=!pA->nData;
	return true;
}
ConstFunc(ETN_OP_NOT,CV_SHORT)
{
	pA->nData=!pA->nData;
	return true;
}
ConstFunc(ETN_OP_NOT,CV_INT)
{
	pA->nData=!pA->nData;
	return true;
}
ConstFunc(ETN_OP_NOT,CV_FLOAT)
{
	*pA=(int)!*((float *)&pA->nData);
	return true;
}
ConstFunc(ETN_OP_NOT,CV_DOUBLE)
{
	*pA=(int)!*((double *)&pA->nData);
	return true;
}
ConstFunc(ETN_OP_BITNOT,CV_CHAR)
{
	pA->nData=~pA->nData;
	return true;
}
ConstFunc(ETN_OP_BITNOT,CV_SHORT)
{
	pA->nData=~pA->nData;
	return true;
}
ConstFunc(ETN_OP_BITNOT,CV_INT)
{
	pA->nData=~pA->nData;
	return true;
}
ConstFunc(ETN_OP_BITNOT,CV_FLOAT)
{
	*pA=~(int)*((float *)&pA->nData);
	return true;
}
ConstFunc(ETN_OP_BITNOT,CV_DOUBLE)
{
	*pA=~(int)*((double *)&pA->nData);
	return true;
}






//+,-
ConstFunc(ETN_OP_PLUS,CV_INT)
{
	pA->nData+=pB->nData;
	return true;
}
ConstFunc(ETN_OP_PLUS,CV_FLOAT)
{
	*((float *)&pA->nData)+=*((float *)&pB->nData);
	return true;
}
ConstFunc(ETN_OP_PLUS,CV_DOUBLE)
{
	*((double *)&pA->nData)+=*((double *)&pB->nData);
	return true;
}
ConstFunc(ETN_OP_MINUS,CV_INT)
{
	pA->nData-=pB->nData;
	return true;
}
ConstFunc(ETN_OP_MINUS,CV_FLOAT)
{
	*((float *)&pA->nData)-=*((float *)&pB->nData);
	return true;
}
ConstFunc(ETN_OP_MINUS,CV_DOUBLE)
{
	*((double *)&pA->nData)-=*((double *)&pB->nData);
	return true;
}



//*,/,%
ConstFunc(ETN_OP_MUL,CV_INT)
{
	pA->nData*=pB->nData;
	return true;
}
ConstFunc(ETN_OP_MUL,CV_FLOAT)
{
	*((float *)&pA->nData)*=*((float *)&pB->nData);
	return true;
}
ConstFunc(ETN_OP_MUL,CV_DOUBLE)
{
	*((double *)&pA->nData)*=*((double *)&pB->nData);
	return true;
}
ConstFunc(ETN_OP_DIV,CV_INT)
{
	pA->nData/=pB->nData;
	return true;
}
ConstFunc(ETN_OP_DIV,CV_FLOAT)
{
	*((float *)&pA->nData)/=*((float *)&pB->nData);
	return true;
}
ConstFunc(ETN_OP_DIV,CV_DOUBLE)
{
	*((double *)&pA->nData)/=*((double *)&pB->nData);
	return true;
}
ConstFunc(ETN_OP_MOD,CV_INT)
{
	pA->nData%=pB->nData;
	return true;
}
ConstFunc(ETN_OP_MOD,CV_FLOAT)
{
	*pA=(int)*((float *)&pA->nData) % (int)*((float *)&pB->nData);
	return true;
}
ConstFunc(ETN_OP_MOD,CV_DOUBLE)
{
	*pA=(int)*((double *)&pA->nData) % (int)*((double *)&pB->nData);
	return true;
}


//&&,||
ConstFunc(ETN_OP_LAND,CV_INT)
{
	pA->nData=(int)(pA->nData && pB->nData);
	return true;
}
ConstFunc(ETN_OP_LOR,CV_INT)
{
	pA->nData=(int)(pA->nData || pB->nData);
	return true;
}


//==,!=,<,>,>=,<=
ConstFunc(ETN_OP_EQUAL,CV_INT)
{
	pA->nData=(int)(pA->nData==pB->nData);
	return true;
}
ConstFunc(ETN_OP_EQUAL,CV_FLOAT)
{
	*pA=(int)(pA->nData==pB->nData);
	return true;
}
ConstFunc(ETN_OP_EQUAL,CV_DOUBLE)
{
	*pA=(int)(pA->nData==pB->nData);
	return true;
}
ConstFunc(ETN_OP_NOTEQUAL,CV_INT)
{
	pA->nData=(int)(pA->nData!=pB->nData);
	return true;
}
ConstFunc(ETN_OP_NOTEQUAL,CV_FLOAT)
{
	*pA=(int)(pA->nData!=pB->nData);
	return true;
}
ConstFunc(ETN_OP_NOTEQUAL,CV_DOUBLE)
{
	*pA=(int)(pA->nData!=pB->nData);
	return true;
}
ConstFunc(ETN_OP_LESS,CV_INT)
{
	pA->nData=(int)(pA->nData<pB->nData);
	return true;
}
ConstFunc(ETN_OP_LESS,CV_FLOAT)
{
	*pA=(int)(*((float *)&pA->nData)<*((float *)&pB->nData));
	return true;
}
ConstFunc(ETN_OP_LESS,CV_DOUBLE)
{
	*pA=(int)(*((double *)&pA->nData)<*((double *)&pB->nData));
	return true;
}
ConstFunc(ETN_OP_GATE,CV_INT)
{
	pA->nData=(int)(pA->nData>pB->nData);
	return true;
}
ConstFunc(ETN_OP_GATE,CV_FLOAT)
{
	*pA=(int)(*((float *)&pA->nData)>*((float *)&pB->nData));
	return true;
}
ConstFunc(ETN_OP_GATE,CV_DOUBLE)
{
	*pA=(int)(*((double *)&pA->nData)>*((double *)&pB->nData));
	return true;
}
ConstFunc(ETN_OP_LESSEQ,CV_INT)
{
	pA->nData=(int)(pA->nData<=pB->nData);
	return true;
}
ConstFunc(ETN_OP_LESSEQ,CV_FLOAT)
{
	*pA=(int)(*((float *)&pA->nData)<=*((float *)&pB->nData));
	return true;
}
ConstFunc(ETN_OP_LESSEQ,CV_DOUBLE)
{
	*pA=(int)(*((double *)&pA->nData)<=*((double *)&pB->nData));
	return true;
}
ConstFunc(ETN_OP_GATEEQ,CV_INT)
{
	pA->nData=(int)(pA->nData>=pB->nData);
	return true;
}
ConstFunc(ETN_OP_GATEEQ,CV_FLOAT)
{
	*pA=(int)(*((float *)&pA->nData)>=*((float *)&pB->nData));
	return true;
}
ConstFunc(ETN_OP_GATEEQ,CV_DOUBLE)
{
	*pA=(int)(*((double *)&pA->nData)>=*((double *)&pB->nData));
	return true;
}


//&,|,^
ConstFunc(ETN_OP_AND,CV_INT)
{
	pA->nData&=pB->nData;
	return true;
}
ConstFunc(ETN_OP_AND,CV_FLOAT)
{
	*pA=(int)*pA & (int)*pB;
	return true;
}
ConstFunc(ETN_OP_AND,CV_DOUBLE)
{
	*pA=(int)*pA & (int)*pB;
	return true;
}
ConstFunc(ETN_OP_OR,CV_INT)
{
	pA->nData|=pB->nData;
	return true;
}
ConstFunc(ETN_OP_OR,CV_FLOAT)
{
	*pA=(int)*pA | (int)*pB;
	return true;
}
ConstFunc(ETN_OP_OR,CV_DOUBLE)
{
	*pA=(int)*pA | (int)*pB;
	return true;
}
ConstFunc(ETN_OP_XOR,CV_INT)
{
	pA->nData^=pB->nData;
	return true;
}
ConstFunc(ETN_OP_XOR,CV_FLOAT)
{
	*pA=(int)*pA ^ (int)*pB;
	return true;
}
ConstFunc(ETN_OP_XOR,CV_DOUBLE)
{
	*pA=(int)*pA ^ (int)*pB;
	return true;
}


//>>,<<
ConstFunc(ETN_OP_ASL,CV_INT)
{
	pA->nData<<=pB->nData;
	return true;
}
ConstFunc(ETN_OP_ASL,CV_FLOAT)
{
	*pA=(int)*pA << (int)*pB;
	return true;
}
ConstFunc(ETN_OP_ASL,CV_DOUBLE)
{
	*pA=(int)*pA << (int)*pB;
	return true;
}
ConstFunc(ETN_OP_ASR,CV_INT)
{
	pA->nData>>=pB->nData;
	return true;
}
ConstFunc(ETN_OP_ASR,CV_FLOAT)
{
	*pA=(int)*pA >> (int)*pB;
	return true;
}
ConstFunc(ETN_OP_ASR,CV_DOUBLE)
{
	*pA=(int)*pA >> (int)*pB;
	return true;
}



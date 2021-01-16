#include<cstdio>
#include<cstdlib>
FILE*midifp,*omidifp;
char*edata;
short type,trks,ppnq;
off_t trkend,sposi,cposi;
static int stat[128],dc=0,kc=0,thr;
unsigned short btyp,typ,chn,num,val;
unsigned int edsize,tick,atick,tksz;
unsigned int revu32(unsigned int x)
{
	return x>>24|(x&0xFF0000)>>8|(x&0xFF00)<<8|x<<24;
}
unsigned int gvln(short x)
{
	unsigned int y=x&0x7F;
	while(x&0x80)
	{
		fread(&x,1,1,midifp);
		(y<<=7)|=x&0x7F;
	}
	return y;
}
void pvln(unsigned int x)
{
	short p=5;
	char tmp[5];
	do
	{
		tmp[--p]=x&0x7F;
		if(p<4)
			tmp[p]|=0x80;
	}while(x>>=7);
	fwrite(tmp+p,5-p,1,omidifp);
}
int main(int oc,char*os[])
{
	unsigned int tmp=0;
	if(oc!=4)
	{
		puts("###Qishipai's Deblacker###");
		puts("用法：");
		printf("\t%s <输入文件(*.mid)> <输出文件(*.mid)> <阈值(整数)>\n",os[0]);
		return 1;
	}
	if(sscanf(os[3],"%d",&thr)!=1)
	{
		printf("%s : 无效阈值\n",os[3]);
		return -1;
	}
	midifp=fopen(os[1],"rb");
	if(!midifp)
	{
		puts("指定MIDI文件不存在！");
		return -1;
	}
	omidifp=fopen(os[2],"wb");
	if(!omidifp)
	{
		puts("无法创建指定输出文件！");
		fclose(midifp);
		return -1;
	}
	fread(&tmp,4,1,midifp);
	if(tmp!=0x6468544Du)
	{
		puts("指定输入不是MIDI文件！");
		fclose(midifp);
		fclose(omidifp);
		return -1;
	}
	fwrite(&tmp,4,1,omidifp);
	fread(&tmp,4,1,midifp);
	fwrite(&tmp,4,1,omidifp);
	trkend=ftello(midifp)+revu32(tmp);
	fread(&(tmp=0),2,1,midifp);
	type=tmp<<8|tmp>>8;
	fread(&tmp,2,1,midifp);
	trks=tmp<<8|tmp>>8;
	fread(&tmp,2,1,midifp);
	ppnq=tmp<<8|tmp>>8;
	/*
		在这里修改头信息
	*/
	tmp=type<<8|type>>8;
	fwrite(&tmp,2,1,omidifp);
	tmp=trks<<8|trks>>8;
	fwrite(&tmp,2,1,omidifp);
	tmp=ppnq<<8|ppnq>>8;
	fwrite(&tmp,2,1,omidifp);
	for(short t=0;t<trks;++t)
	{
		for(int i=0;i<128;++i)
			stat[i]=0;
		atick=tmp=btyp=typ=chn=0;
		fseeko(midifp,trkend,SEEK_SET);
		fread(&tmp,4,1,midifp);
		if(tmp!=0x6B72544Du)
		{
			puts("MIDI轨道头信息错误！");
			fclose(midifp);
			fclose(omidifp);
			return -1;
		}
		fwrite(&tmp,4,1,omidifp);
		sposi=ftello(omidifp);
		fwrite("\0\0\0\0",4,1,omidifp);
		fread(&tmp,4,1,midifp);
		trkend=ftello(midifp)+revu32(tmp);
		while(ftello(midifp)<trkend)
		{
			tick=gvln(0x80);
			fread(&(tmp=0),1,1,midifp);
			if(tmp&0x80)
			{
				typ=tmp==0xFF?0xFF:tmp&0xF0;
				chn=tmp==0xFF?0:tmp&0xF;
				fread(&tmp,1,1,midifp);
			}
			switch(typ)
			{
			case(0x80):case(0x90):
			case(0xA0):case(0xB0):
				num=tmp,val=0;
				fread(&val,1,1,midifp);
				break;
			case(0xC0):case(0xD0):
				val=tmp;
				break;
			case(0xE0):
				val=tmp<<7;
				fread(&tmp,1,1,midifp);
				val|=tmp;
				break;
			case(0xFF):
				num=tmp;
				fread(&tmp,1,1,midifp);
			case(0xF0):
				edata=new char[edsize=gvln(tmp)];
				fread(edata,edsize,1,midifp);
				break;
			default:
				puts("发现无法解析的midi事件！");
				fclose(midifp);
				fclose(omidifp);
				return -1;
			}
			tick+=atick;
			if(typ==0x90)
			{
				if(val>thr)
					++kc;
				else
				{
					++dc,++stat[num],atick=tick;
					continue;
				}
			}
			if(typ==0x80&&stat[num]>0)
			{
				--stat[num],atick=tick;
				continue;
			}
			atick=0;
			pvln(tick);
			if((typ&0xF0)==0xF0||typ!=btyp)
			{
				btyp=typ,tmp=typ|chn;
				fwrite(&tmp,1,1,omidifp);
			}
			switch(typ)
			{
			case(0x80):case(0x90):
			case(0xA0):case(0xB0):
				fwrite(&num,1,1,omidifp);
			case(0xC0):case(0xD0):
				fwrite(&val,1,1,omidifp);
				break;
			case(0xE0):
				fwrite(&(tmp=val>>7),1,1,omidifp);
				fwrite(&(tmp=val&0x7F),1,1,omidifp);
				break;
			case(0xFF):
				fwrite(&num,1,1,omidifp);
			case(0xF0):
				pvln(edsize);
				fwrite(edata,edsize,1,omidifp);
			}
			if((typ&0xF0)==0xF0)
				delete[]edata;
		}
		cposi=ftello(omidifp);
		tksz=revu32(cposi-sposi-4);
		fseeko(omidifp,sposi,SEEK_SET);
		fwrite(&tksz,4,1,omidifp);
		fseeko(omidifp,cposi,SEEK_SET);
		printf("track%5d /%5d finish\r",t+1,trks);
		fflush(stdout);
	}
	printf("\n%9d delete\n%9d keep\n",dc,kc);
	fclose(midifp);
	fclose(omidifp);
	return 0;
}

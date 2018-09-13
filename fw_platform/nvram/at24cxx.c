/*
 * AT24Cxx i2c EEPROM nvram driver
 *
 * Copyright 2018 Hunan GreatWall Information Financial Equipment Co., Ltd.
 *
 */

#include <stdint.h>
#include <string.h>

#include "nvram.h"
#include "at24cxx.h"
#include "mss_i2c.h"
#include "mss_assert.h"

/*------------------------------------------------------------------------------
 * i2c master serial address.
 */
#define MASTER_SER_ADDR     0x21
/*-----------------------------------------------------------------------------
 * i2c operation timeout value in mS. Define as MSS_I2C_NO_TIMEOUT to disable
 * the timeout functionality.
 */
#define DEMO_I2C_TIMEOUT 3000u


static int at24cxx_erase(struct nvram *nvm)
{
	return 0;
}
static mss_i2c_status_t at24cxx_wait_complete(mss_i2c_instance_t * this_i2c,uint32_t timeout_ms)
{
	uint16_t i=0;
	mss_i2c_status_t i2c_status;
	
	ASSERT( (this_i2c == &g_mss_i2c0) || (this_i2c == &g_mss_i2c1) );
	
	this_i2c->master_timeout_ms = timeout_ms;

	/* Run the loop until state returns I2C_FAILED	or I2C_SUCESS*/
	do {
		i2c_status = this_i2c->master_status;
		i++;
		if(i==0xff)
		{
			MSS_I2C_system_tick(this_i2c, 1);
			i=0;
		}
	} while(MSS_I2C_IN_PROGRESS == i2c_status);
	return i2c_status;
}


static void delay_at24c(void)
{
	uint16_t i;
	/*Write cycle>=5ms*/
	for(i=0;i<0x1fff;i++);
}
static int at24cxx_page_write(struct nvram *nvm, const void *buffer, int offset, int count)
{
	struct at24cxx_resource *nvm_rc;
	int cnt,i;
	uint8_t serial_address,data_address;
	uint8_t wrbuf[256]={0};
	const uint8_t* pBuffer;
	nvm_rc = (struct at24cxx_resource *)nvm->resource;
	pBuffer=(const uint8_t*)buffer;
	if(offset>=nvm_rc->size)
	{
		return -1;
	}	
	cnt = (count + offset <= nvm_rc->size) ? count : (nvm_rc->size - offset);

	serial_address=(uint8_t)(offset>>8);
	serial_address=serial_address &0x07;
	serial_address=(nvm_rc->i2c_address>>1)|serial_address;
	data_address=(uint8_t)(offset&0xff);
	wrbuf[0]=data_address;
	for(i=0;i<cnt;i++)
		wrbuf[i+1]=*pBuffer++;
	MSS_I2C_write(nvm_rc->i2c,serial_address,wrbuf,cnt+1,MSS_I2C_RELEASE_BUS);
	if(at24cxx_wait_complete(nvm_rc->i2c, DEMO_I2C_TIMEOUT)!=MSS_I2C_SUCCESS)
	{
		return -1;
	}
	return cnt;	
}
static int at24cxx_read(struct nvram *nvm, void *buffer, int offset, int count)
{
	struct at24cxx_resource *nvm_rc;
	int cnt;
	uint8_t serial_address,data_address;
	nvm_rc = (struct at24cxx_resource *)nvm->resource;
	if(offset>=nvm_rc->size)
	{
		return -1;
	}
	cnt = (count + offset <= nvm_rc->size) ? count : (nvm_rc->size - offset);
	serial_address=(uint8_t)(offset>>8);
	serial_address=serial_address &0x07;
	serial_address=(nvm_rc->i2c_address>>1)|serial_address;
	data_address=(uint8_t)(offset&0xff);
	/* Send EEPROM address for write*/
	MSS_I2C_write(nvm_rc->i2c,serial_address,&data_address,1,MSS_I2C_HOLD_BUS);
	if(at24cxx_wait_complete(nvm_rc->i2c, DEMO_I2C_TIMEOUT)!=MSS_I2C_SUCCESS)
	{
		return -1;
	}
	/* Send EEPROM address for read*/
	MSS_I2C_read(nvm_rc->i2c,serial_address,buffer,cnt,MSS_I2C_RELEASE_BUS);
	if(at24cxx_wait_complete(nvm_rc->i2c, DEMO_I2C_TIMEOUT)!=MSS_I2C_SUCCESS)
	{
		return -1;
	}
	return cnt;
}


static int at24cxx_write(struct nvram *nvm, const void *buffer, int offset, int count)
{
	struct at24cxx_resource *nvm_rc;
	int cnt;
	uint16_t addr;  //��ǰд���ַ
	uint8_t page;  //��Ҫ���ٸ�����ҳ
	uint8_t head; //��ͷ��ҳ���벿���ж��ٸ��ֽ�
	uint8_t rear;  //��β��ҳ���벿���ɶ��ٸ��ֽ�
	uint8_t i;
	const uint8_t* pBuffer;
	pBuffer=(const uint8_t*)buffer;
	nvm_rc = (struct at24cxx_resource *)nvm->resource;
	if(offset>=nvm_rc->size)
	{
		return -1;
	}
	cnt = (count + offset <= nvm_rc->size) ? count : (nvm_rc->size - offset);
	addr= offset%(nvm_rc->pagesize);  //����д�������ҳ�е��ĸ��ֽڿ�ʼ Ϊ0ʱ��Ϊҳ����
	head=(nvm_rc->pagesize)-addr;		 //��һҳ����д����ٸ��ֽڡ�
	if(cnt<=head)                        //�����һҳ����д����������
	{
		head=cnt;
		page=0;
		rear=0;
	}	
	else	 //�����Ҫ��ҳ����㹲��Ҫ���ٸ�����ҳ���Լ����һҳ��Ҫд���ٸ��ֽڡ�
	{
		page=(cnt-head)/(nvm_rc->pagesize);
		rear=(cnt-head)%(nvm_rc->pagesize);
	}	
	addr=offset;
	//д��һҳ������
	if(head)   //������ж��Ƿ�ֹNUM����0�������
	{
		at24cxx_page_write(nvm,pBuffer,addr,head);
		addr+=head;   //�����´�д���ַ
		pBuffer+=head;	//�����´�д�����ʼ����
		
	}
	//�м����ҳд��
	for(i=0;i<page;i++)
	{
		delay_at24c();
		at24cxx_page_write(nvm,pBuffer,addr,nvm_rc->pagesize);
		addr+=nvm_rc->pagesize;	//�����´�д���ַ
		pBuffer+=nvm_rc->pagesize; //�����´�д�����ʼ����
	}
	//��β��ʣ���ֽ�д�롣
	if(rear)
	{
		delay_at24c();
		at24cxx_page_write(nvm,pBuffer,addr,rear);
	}
	return cnt;
}


int at24cxx_install(struct nvram *nvm)
{
	struct at24cxx_resource *nvm_rc;

	if (!nvm)
		return -1;
	if (!nvm->resource)
		return -1;

	nvm_rc = (struct at24cxx_resource *)nvm->resource;
	// set nvram features
	nvm->feature.size = nvm_rc->size;
	nvm->feature.pagesize = nvm_rc->pagesize;

	// set nvram operation functions
	nvm->erase = at24cxx_erase;
	nvm->read = at24cxx_read;
	nvm->write = at24cxx_write;
	MSS_I2C_init(nvm_rc->i2c, MASTER_SER_ADDR, MSS_I2C_PCLK_DIV_60);
	return 0;
}


int at24cxx_drvinit(void)
{
	return 0;
}


/**************************
	test.c
	gcc test.c -ljpeg
	./a.out
*****************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeglib.h"
#include "jerror.h"
#include <time.h>

#include <pthread.h>//需要添加pthread.h头文件

clock_t start, stop;
/* clock_t是clock()函数返回的变量类型 */
double duration;

/**************************
jpg图片进行解压，然后存放到内存中
jpeg-->rgb
**********************/

void *memcpy_rgb2y(char *dest, const char *src, size_t count){
	// char *d = (char*)dest;
	// const char *s = (char*)src;
	while(count--){
		// *d++ = *s++; //*s++ = *(s++)
		// *d++ = ((*s++)*1224+(*s++)*2404+(*s++)*467)>>12;
		*dest++ = ((*src++)*1224+(*src++)*2404+(*src++)*467)>>12;
	}
}
unsigned long jpeg_imageFile_dec_rgb2y_enc(char *inbuff, size_t inbuff_len){
	int quality = 60;
	int width = 1920;
	int height = 1080;
	// -----------INPUT 参数在上面
	struct jpeg_decompress_struct cinfo;
	JSAMPARRAY raw_buffer;
	char *src_buff; // 需要释放
	/**分配数据空间*/
	src_buff = (char *)malloc(width*height);//ywz
	/**定义一个标准的错误结构体，一旦程序出现错误就会调用exit()函数，退出进程**/
	struct jpeg_error_mgr jpeg_err;
	cinfo.err = jpeg_std_error(&jpeg_err);
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, inbuff, inbuff_len);
	jpeg_read_header(&cinfo, TRUE);
	/**开始解压缩**/
	jpeg_start_decompress(&cinfo);
	raw_buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,JPOOL_IMAGE, width*3, 1);
	/***图像宽高，深度**/
	// width = cinfo.output_width;
	// height = cinfo.output_height;
	// depth = cinfo.output_components;
	char *ptr_src_buff = src_buff;
	while(cinfo.output_scanline < height){
		/***读取一行jpeg数据到raw_buffer***/
		jpeg_read_scanlines(&cinfo, raw_buffer, 1);
		// Y = 0.299R+0.587G+0.114B
		/**将raw_buffer中的数据给src_buff***/
		// memcpy(ptr_src_buff, *raw_buffer, width); //直传
		memcpy_rgb2y(ptr_src_buff, *raw_buffer, width); // rgb2y
		/***指针偏移一行***/
		ptr_src_buff += width;
	}

	/***解压缩完毕***/
	jpeg_finish_decompress(&cinfo);
	/***释放资源***/
	jpeg_destroy_decompress(&cinfo);

	// ywz 第二阶段 src_buff再编码：
	/**定义一个标准的错误结构体，一旦程序出现错误就会调用exit()函数，退出进程**/
	unsigned long jpeg_size;
	struct jpeg_error_mgr jpeg_err_enc;
	struct jpeg_compress_struct cinfo_enc;
	/***绑定错误处理结构对象**/
	cinfo_enc.err = jpeg_std_error(&jpeg_err_enc);
	jpeg_create_compress(&cinfo_enc);

	/****1. 指定目标源***/
	jpeg_mem_dest(&cinfo_enc, &inbuff, &jpeg_size);
	/**2. 指定图像宽高，深度，色彩空间***/
	cinfo_enc.image_width = width;
	cinfo_enc.image_height = height;
	cinfo_enc.input_components = 1;
	cinfo_enc.in_color_space = JCS_GRAYSCALE;//JCS_GRAYSCALE; ;//JCS_RGB;
	// cinfo_enc.num_components = depth;
	// cinfo_enc.data_precision = 8;
	/**设置jpeg默认值**/
	jpeg_set_defaults(&cinfo_enc);
	/**设置图片质量***/
	jpeg_set_quality(&cinfo_enc, quality, TRUE); // quality
	jpeg_start_compress(&cinfo_enc, TRUE);
	while(cinfo_enc.next_scanline < cinfo_enc.image_height){
		raw_buffer = &src_buff[cinfo_enc.next_scanline * width];
		jpeg_write_scanlines(&cinfo_enc, &raw_buffer, 1);
	}

	/**图像压缩结束**/
	jpeg_finish_compress(&cinfo_enc);
	/***销毁图像压缩***/
	jpeg_destroy_compress(&cinfo_enc);
	free(src_buff);
	// ywz 第二阶段done
	return jpeg_size;
}

int main(int argc, char *argv[]){
	printf("hello jpeglib.\n");
	/***quality的值越大，图片数据量越大越清晰，原来为60**/

	// char *outbuff;
	// outbuff = (char *)malloc(1920*1080*3);

	/**指定解压缩数据源**/
	FILE* fp = NULL;
    fp = fopen("11.jpg", "rb");
    unsigned char *inbuff;
	long inbuff_len = 626893;
	inbuff = calloc(inbuff_len, 1);
    fread(inbuff, 1, inbuff_len, fp);
	printf("read done");
    fclose(fp);


	/**jpg图片进行解码***/
	start = clock();    /* 开始计时 */
	unsigned long jpeg_size = jpeg_imageFile_dec_rgb2y_enc(inbuff, inbuff_len);
	stop = clock();    /* 停止计时 */
    duration = ((double)(stop - start))/CLOCKS_PER_SEC;
	printf("cost-time:%f\n", duration);

	printf("done together");

	printf("write len:%d", jpeg_size);

	fp = fopen("out.jpg", "wb");
    fwrite(inbuff, 1, jpeg_size, fp);
    fclose(fp);
    printf("write done");
	printf("decode done outsider: %d", jpeg_size);

	/**释放内存**/
	free(inbuff);
	// free(outbuff);

	return 0;
}





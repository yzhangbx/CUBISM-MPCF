/*
 *  WaveletTexture3D.h
 *  
 *
 *  Created by Diego Rossinelli on 3/28/13.
 *  Copyright 2013 ETH Zurich. All rights reserved.
 *
 */

#pragma once

#include <string>
#include <vector>
#include <sstream>

using namespace std;

#include <mpi.h>

#include "../../MPCFnode/source/WaveletCompressor.h"

#ifndef MYASSERT
//MACRO TAKEN FROM http://stackoverflow.com/questions/3767869/adding-message-to-assert
#   define MYASSERT(condition, message) \
do { \
if (! (condition)) { \
std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
<< " line " << __LINE__ << ": " << message << std::endl; \
std::exit(EXIT_FAILURE); \
} \
} while (false)
#endif

typedef WaveletCompressorGeneric_zlib<_VOXELS_, float> TextureCompressor;

struct WaveletTexture3D
{
	struct Geometry
	{
		float pos[3], size[3]; 
		float texcoordstart[3],texcoordend[3];
		
		template<int dim>
		void setup(const int gstart, const int gend, const int ghost1side, const double gridspacing)
		{			
			pos[dim] = gstart * gridspacing;
			size[dim] = (gend - gstart) * gridspacing;
			
			const double voxelsize = 1. / _VOXELS_;

			texcoordstart[dim] = ghost1side * voxelsize;
			texcoordend[dim] = (gend - gstart - ghost1side) * voxelsize; 
		}		
		
		void print()
		{
			for(int i = 0; i < 3; ++i)
				printf("%d: pos: %f size: %f, tex: %f - %f\n", i, pos[i], size[i], texcoordstart[i], texcoordend[i]);
		}
		
	} geometry;
		
	float data[_VOXELS_][_VOXELS_][_VOXELS_];
	
	TextureCompressor wavcomp;

	//not anymore: void clear() { memset(data, 0, sizeof(data)); }
	
	void compress(const float threshold, const bool halffloat, const unsigned char *& compresseddata, size_t& nbytes)
	{
		nbytes = wavcomp.compress(threshold, halffloat, data);
		compresseddata = (unsigned char *) wavcomp.data();
	}	
	
	void * compression_buffer() { return wavcomp.data(); }
	
	void decompress(const bool halffloat, const size_t nbytes)
	{
		printf("decompressing args: %d %d\n", halffloat, nbytes);
		wavcomp.decompress(halffloat, nbytes, data);
	}
};

class WaveletTexture3D_Collection
{
protected:
	
#pragma pack (push)
#pragma pack(1)
	struct CompressedTexData { WaveletTexture3D::Geometry geometry; size_t start; int nbytes; }; 
#pragma pack(pop)
		
	string path, header, data_title;
	size_t lutfile_start, bufferfile_start, buffertitle_start, buffer_size;
	
	int xtextures, ytextures, ztextures, ntextures;
	float wavelet_threshold;
	bool halffloat;
	
	FILE * myfile;
	
	//this data is used when reading the file
	vector<CompressedTexData> metadata;
	
	void _setup()
	{
		{
			std::stringstream ss;
			
			ss << "\n==============START-ASCI-HEADER==============\n";
			
			{
				int one = 1;
				bool isone = *(char *)(&one);
				
				ss << "Endianess: " << (isone ? "little" : "big") << "\n";
			}
			
			ss << "Voxelsperdimension: " << _VOXELS_ << "\n";
			ss << "Textures: " << xtextures << " x "  << ytextures << " x " << ztextures  << "\n";
			ss << "HalfFloat: " << (this->halffloat ? "yes" : "no") << "\n";
			ss << "Wavelets: " << WaveletsOnInterval::ChosenWavelets_GetName() << "\n";
			ss << "Threshold: " << wavelet_threshold << "\n";
			ss << "Encoder: " << "zlib" << "\n";
			ss << "SizeofCompressedTexData: " << sizeof(CompressedTexData) << "\n";
			ss << "==============START-BINARY-LUT==============\n";
			
			header = ss.str();
		}
		
		data_title = "==============START-BINARY-DATA==============\n";
		
		lutfile_start = header.size();
		buffertitle_start = lutfile_start + ntextures * sizeof(CompressedTexData);
		bufferfile_start = buffertitle_start + data_title.size();
	}
	
public:
	
	WaveletTexture3D_Collection(const string path, bool openfile = true): path(path), buffer_size(0), myfile(NULL)
	{
		_setup();
		
		if (openfile)
		{
			myfile = fopen(path.c_str(), "rb");
			assert(myfile);
			
			char tmp[1024];
			fgets(tmp, sizeof(tmp), myfile);
			fgets(tmp, sizeof(tmp), myfile);
			assert(string(tmp) == "==============START-ASCI-HEADER==============\n");
			
			fscanf(myfile, "Endianess:  %s\n", tmp);
			printf("Endianess: <%s>\n", tmp);
			MYASSERT(string(tmp) == "little", "ooops Endianess is not little!\n");
			
			int voxels = -1;
			fscanf(myfile, "Voxelsperdimension:  %d\n", &voxels);
			printf("Voxelsperdimension: <%d>\n", voxels);
			MYASSERT(voxels == _VOXELS_, "Ooops voxels " << voxels << " instead of " << _VOXELS_ << "\n");

			fscanf(myfile, "Textures: %d x %d x %d\n", &xtextures, &ytextures, &ztextures);
			ntextures = xtextures * ytextures * ztextures;
			printf("Textures: %d x %d x %d\n", xtextures, ytextures, ztextures);

			fscanf(myfile, "HalfFloat: %s\n", tmp);
			printf("HalfFloat: <%s>\n", tmp);
			this->halffloat = (string(tmp) == "yes");
			
			fscanf(myfile, "Wavelets: %s\n", tmp);
			printf("Wavelets: <%s>\n", tmp);
			MYASSERT(tmp == string(WaveletsOnInterval::ChosenWavelets_GetName()),
					 "\nATTENZIONE:\nWavelets in the file is " << tmp << 
					 " and i have " << WaveletsOnInterval::ChosenWavelets_GetName() << "\n");
			
			fscanf(myfile, "Threshold: %e\n", &wavelet_threshold);
			printf("Threshold: %e\n", wavelet_threshold);
			
			fscanf(myfile, "Encoder: %s\n", tmp);
			printf("Encoder: <%s>\n", tmp);
			MYASSERT(tmp == string("zlib"),
					 "\nATTENZIONE:\nWavelets in the file is " << tmp << 
					 " and i have zlib.\n");
			
			int sizeofstruct = -1;	
			fscanf(myfile, "SizeofCompressedTexData:  %d\n", &sizeofstruct);
			printf("SizeofCompressedTexData: <%d>\n", sizeofstruct);
			MYASSERT(sizeof(CompressedTexData) == sizeofstruct,
					 "\nATTENZIONE:\nWavelets in the file is " << sizeofstruct << 
					 " and i have" << sizeof(CompressedTexData) << "\n");

			fgets(tmp, sizeof(tmp), myfile);
			
			assert(string("==============START-BINARY-LUT==============\n") == string(tmp));
			printf("==============END ASCI-HEADER==============\n\n");	
			
			metadata.resize(ntextures);
			fread(&metadata.front(), sizeof(CompressedTexData), ntextures, myfile);
			
			printf("geometry is:\n");
			for(int i=0; i<ntextures; ++i)
			{
				printf("%d: %zd %d\n", i, metadata[i].start, metadata[i].nbytes);
				metadata[i].geometry.print();
			}
		}
	}
	
	int get_xtextures() const { return xtextures; }
	int get_ytextures() const { return ytextures; }
	int get_ztextures() const { return ztextures; }
	
	WaveletTexture3D_Collection(const string path, const int xtextures, const int ytextures, const int ztextures,
								const float wavelet_threshold, const bool halffloat, bool openfile = true): 
	path(path), xtextures(xtextures), ytextures(ytextures), ztextures(ztextures), ntextures(xtextures * ytextures * ztextures),
	wavelet_threshold(wavelet_threshold), halffloat(halffloat), buffer_size(0), myfile(NULL)
	{				
		_setup();
		
		if (openfile)
		{
			myfile = fopen(path.c_str(), "wb");
			assert(myfile);
		
			fwrite(header.c_str(), sizeof(char), header.size(), myfile);
			fseek(myfile, buffertitle_start, SEEK_SET);
			fwrite(data_title.c_str(), sizeof(char), data_title.size(), myfile);				
		}
	}
	
	virtual ~WaveletTexture3D_Collection() 
	{				
		if (myfile)
		{
			fclose(myfile);
			
			myfile = NULL;
			
			printf("Terminating...closing file (%.2f kB) now.\n", (bufferfile_start + buffer_size)/1024.);
		}
	}
	
	virtual void write(const int ix, const int iy, const int iz, WaveletTexture3D& texture)
	{
		//check that we are not totally nuts
		assert(ix >= 0 && ix < xtextures);
		assert(iy >= 0 && iy < ytextures);
		assert(iz >= 0 && iz < ztextures);
		
		//compress the texture
		const unsigned char * ptr = NULL;
		size_t nbytes = 0;
		texture.compress(wavelet_threshold, halffloat, ptr, nbytes);
		
		//spit some output for now
		{
			const size_t uncompressedbytes = sizeof(Real) * _VOXELS_ * _VOXELS_ * _VOXELS_;
			
			printf("Texture %d %d %d. Compression ratio: %.2f (threshold:%.2e)\n", 
				   ix, iy, iz, uncompressedbytes * 1. / nbytes, wavelet_threshold);	
		}
		
		//allocate a region in the file
		const size_t myoffset = bufferfile_start + buffer_size;
		buffer_size += nbytes;
		
		//write lut
		CompressedTexData entry = { texture.geometry, myoffset, nbytes };
		const size_t mylutoffset = sizeof(entry) * (ix + xtextures * (iy + ytextures * iz));
		fseek(myfile, lutfile_start + mylutoffset, SEEK_SET);
		fwrite(&entry, sizeof(entry), 1, myfile);
		
		//write data
		assert(ptr != NULL);
		assert(nbytes != 0);
		fseek(myfile, myoffset, SEEK_SET);
		fwrite(ptr, sizeof(unsigned char), nbytes, myfile);
	}
	
	virtual void read(const int ix, const int iy, const int iz, WaveletTexture3D& texture)
	{
		const int myentry = ix + xtextures * (iy + ytextures * iz);

		//check that we are not totally nuts
		assert(ix >= 0 && ix < xtextures);
		assert(iy >= 0 && iy < ytextures);
		assert(iz >= 0 && iz < ztextures);		
		assert(myentry >= 0 && myentry < metadata.size());

		const size_t nbytes = metadata[myentry].nbytes;
		fseek(myfile, metadata[myentry].start, SEEK_SET);
		printf("reading... %d %d, halffloat: %d\n", nbytes, metadata[myentry].start, halffloat);
		fread(texture.compression_buffer(), sizeof(unsigned char), nbytes, myfile);
		
		//decompress the texture
		texture.decompress(halffloat, nbytes);
		
		//spit some output
		{
			const size_t uncompressedbytes = sizeof(float) * _VOXELS_ * _VOXELS_ * _VOXELS_;
			
			printf("Texture %d %d %d. Compression ratio: %.2f (threshold:%.2e)\n", 
				   ix, iy, iz, uncompressedbytes * 1. / nbytes, wavelet_threshold);	
		}
	}
};


class WaveletTexture3D_CollectionMPI: public WaveletTexture3D_Collection
{	
	size_t * file_offset;
	
	MPI::Win rmawindow;
	MPI::File myfile;
	const MPI::Intracomm& mycomm;

public:
	
	WaveletTexture3D_CollectionMPI(const MPI::Intracomm& comm, 
								   const string path, const int xtextures, const int ytextures, const int ztextures,
								   const float wavelet_threshold, const bool halffloat): 
	mycomm(comm), WaveletTexture3D_Collection(path, xtextures, ytextures, ztextures, wavelet_threshold, halffloat, false), file_offset(NULL)
	{		
		const int mygid = comm.Get_rank();

		//file setup
		{			
			myfile = MPI::File::Open(mycomm, path.c_str(), MPI::MODE_CREATE | MPI::MODE_WRONLY, MPI::INFO_NULL);
						
			if (mygid == 0)
			{
				myfile.Write_at(0, header.c_str(), header.size(), MPI::CHAR);
				myfile.Write_at(buffertitle_start, data_title.c_str(), data_title.size(), MPI::CHAR);				
			}			
		}
		
		//one sided communication setup
		{
			file_offset = (size_t *) MPI::Alloc_mem(sizeof(size_t), MPI::INFO_NULL);
			*file_offset = bufferfile_start; 
			assert(*file_offset != 0);
			
			if (mygid == 0)
				printf("at the beginning my offset was %zd\n", *file_offset);
			
			rmawindow = MPI::Win::Create(file_offset, sizeof(size_t), sizeof(size_t), MPI::INFO_NULL, mycomm);
		}
	}
	
	~WaveletTexture3D_CollectionMPI() 
	{		
		rmawindow.Free();
		
		if (mycomm.Get_rank() == 0)
			printf("Terminating...closing file (%.2f kB) now.\n", *file_offset/1024.);
		
		MPI::Free_mem(file_offset);
		
		myfile.Close(); 		
	}
		
	void write(const int ix, const int iy, const int iz, WaveletTexture3D& texture)
	{
		//check that we are not totally nuts
		assert(ix >= 0 && ix < xtextures);
		assert(iy >= 0 && iy < ytextures);
		assert(iz >= 0 && iz < ztextures);

		//compress the texture
		const unsigned char * ptr = NULL;
		size_t nbytes = 0;
		texture.compress(wavelet_threshold, halffloat, ptr, nbytes);

		//spit some output for now
		{
			const size_t uncompressedbytes = sizeof(float) * _VOXELS_ * _VOXELS_ * _VOXELS_;
			
			printf("Texture %d %d %d. Compression ratio: %.2f (threshold:%.2e)\n", 
				   ix, iy, iz, uncompressedbytes * 1. / nbytes, wavelet_threshold);	
		}

		//obtain file offset
		size_t myoffset = 0;
		{
			rmawindow.Lock(MPI::LOCK_EXCLUSIVE, 0, 0);
			rmawindow.Get(&myoffset, 1, MPI_UINT64_T, 0, 0, 1, MPI_UINT64_T); 
			rmawindow.Accumulate(&nbytes, 1, MPI_UINT64_T, 0, 0, 1, MPI_UINT64_T, MPI::SUM);
			rmawindow.Unlock(0);
		}
		assert(myoffset != 0);
		printf("myoffset is %zd\n", myoffset);

		//write lut
		CompressedTexData entry = { texture.geometry, myoffset, nbytes };
		
		const size_t mylutoffset = sizeof(entry) * (ix + xtextures * (iy + ytextures * iz));
		myfile.Write_at(lutfile_start + mylutoffset, &entry, sizeof(entry), MPI::CHAR);

		//write data
		assert(ptr != NULL);
		assert(nbytes != 0);
		myfile.Write_at(myoffset, ptr, nbytes, MPI::CHAR);
	}
};

/****
DIAMOND protein aligner
Copyright (C) 2020 QIAGEN A/S (Aarhus, Denmark)
Code developed by Patrick Ettenhuber <patrick.ettenhuber@qiagen.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#pragma once
#include <Eigen/Sparse>
#include <stdexcept>
#include <string>
#include <stdio.h>
#include <memory>
#include <fstream>
#include <iostream>
#include "disjoint_set.h"
#include "../util/io/consumer.h"
#include "cluster.h"

using namespace std;

namespace Workflow { namespace Cluster{
template <typename T>
class SparseMatrixStream : public Consumer {
	struct CoordinateCmp {
		bool operator()(const Eigen::Triplet<T>& lhs, const Eigen::Triplet<T>& rhs) const { 
			return lhs.row() == rhs.row() ? lhs.col() < rhs.col() : lhs.row() < rhs.row() ; 
		}
	};
	size_t n;
	float max_size;
	set<Eigen::Triplet<T>, CoordinateCmp> data;
	LazyDisjointSet<uint32_t>* disjointSet;
	ofstream* os;
	inline void write_triplet(const uint32_t* const query, const uint32_t* const subject, const T* const value){
		if(os){
			os->write((char*) query, sizeof(uint32_t));
			os->write((char*) subject, sizeof(uint32_t));
			double val = *value;
			os->write((char*) &val, sizeof(double));
		}
	}

	vector<vector<Eigen::Triplet<T>>> split_data(map<uint32_t, uint32_t>& indexToSetId, size_t size){
		vector<vector<Eigen::Triplet<T>>> split(size);
		for(Eigen::Triplet<T> t : data){
			uint32_t iset = indexToSetId[t.row()];
			assert( iset == indexToSetId[t.col()]);
			split[iset].emplace_back(t.row(), t.col(), t.value());
		}
		return split;
	}

	void dump(){
		if(os && data.size() > 0){
			vector<vector<uint32_t>> indices = get_indices();
			map<uint32_t, uint32_t> indexToSetId;
			for(uint32_t iset = 0; iset < indices.size(); iset++){
				for(uint32_t index : indices.at(iset)){
					indexToSetId.emplace(index, iset);
				}
			}
			vector<vector<Eigen::Triplet<T>>> components = split_data(indexToSetId, indices.size());
			for(uint32_t iComponent = 0; iComponent < components.size(); iComponent++){
				const uint32_t size = components[iComponent].size();
				if( size > 0 ){
					const uint32_t first_index = indices[iComponent][0];
					os->write((char*) &first_index, sizeof(uint32_t));
					os->write((char*) &size, sizeof(uint32_t));
					for(auto& t : components[iComponent]){
						const uint32_t irow = t.row();
						const uint32_t icol = t.col();
						const T val = t.value();
						write_triplet(&irow, &icol, &val);
					}
				}
			}
		}
	}
	virtual void consume(const char *ptr, size_t n) override {
		const char *end = ptr + n;
		if (n >= sizeof(uint32_t)) {
			while (ptr  < end) {
				const uint32_t query = *(uint32_t*) ptr;
				ptr += sizeof(uint32_t);
				const uint32_t subject = *(uint32_t*) ptr;
				ptr += sizeof(uint32_t);
				const double value = *(double*) ptr;
				ptr += sizeof(double);
				Eigen::Triplet<T> t(query, subject, value);
				auto it = data.find(t);
				if(it == data.end()){
					data.emplace(move(t));
					disjointSet->merge(query, subject);
				}
				else if (t.value() > it->value()){
					data.emplace_hint(data.erase(it), move(t));
				}
				if(os && (data.size()*2.0*sizeof(uint32_t)+sizeof(double))/(1024ULL*1024*1024) >= max_size){
					dump();
					data.clear();
				}
			}
		}
	}

	void build_graph(const char *ptr, size_t n) {
		const char *end = ptr + n;
		if (n >= sizeof(uint32_t)) {
			while (ptr  < end) {
				const uint32_t query = *(uint32_t*) ptr;
				ptr += sizeof(uint32_t);
				const uint32_t subject = *(uint32_t*) ptr;
				ptr += sizeof(uint32_t);
				ptr += sizeof(double);
				disjointSet->merge(query, subject);
			}
		}
	}

	ofstream* getStream(string graph_file_name){
		if(graph_file_name.empty()){
			return nullptr;
		}
		ofstream* os = new ofstream(graph_file_name, ios::out | ios::binary);
		os->write((char*) &n, sizeof(size_t));
		const uint32_t indexversion = 0;
		os->write((char*) &indexversion, sizeof(uint32_t));
		return os;
	}

	vector<Eigen::Triplet<T>> remap(vector<Eigen::Triplet<T>>& split, map<uint32_t, uint32_t>& index_map){
		vector<Eigen::Triplet<T>> remapped;
		for(Eigen::Triplet<T> const & t : split){
			remapped.emplace_back(index_map[t.row()], index_map[t.col()], t.value());
		}
		remapped.shrink_to_fit();
		return remapped;
	}

	vector<vector<Eigen::Triplet<T>>> getComponents(vector<vector<uint32_t>*>* indices){
		vector<vector<Eigen::Triplet<T>>> split;
		{
			map<uint32_t, uint32_t> indexToSetId;
			for(uint32_t iset = 0; iset < indices->size(); iset++){
				for(uint32_t index : *(indices->at(iset))){
					indexToSetId.emplace(index, iset);
				}
			}
			split = split_data(indexToSetId, indices->size());
		}
		vector<vector<Eigen::Triplet<T>>> components;
		for(uint32_t iset = 0; iset < indices->size(); iset++){
			if(split[iset].size() > 0){
				map<uint32_t, uint32_t> index_map;
				uint32_t iel = 0;
				for(uint32_t const & el: *(indices->at(iset))){
					index_map.emplace(el, iel++);
				}
				components.emplace_back(remap(split[iset], index_map));
			}
		}
		return components;
	}

public:
	SparseMatrixStream(size_t n, string graph_file_name){
		this->n = n;
		disjointSet = new LazyDisjointIntegralSet<uint32_t>(n); 
		this->max_size = 2.0; // 2 GB default flush
		os = getStream(graph_file_name);
	}
	SparseMatrixStream(unordered_set<uint32_t>* set){
		this->n = set->size();
		disjointSet = new LazyDisjointTypeSet<uint32_t>(set); 
		this->max_size = 2.0; // 2 GB default flush
		os = nullptr;
	}
	SparseMatrixStream(size_t n){
		this->n = n;
		disjointSet = new LazyDisjointIntegralSet<uint32_t>(n); 
		os = nullptr;
	}

	~SparseMatrixStream(){
		dump();
		data.clear();
		if(disjointSet) delete disjointSet;
		if(os){
			os->close();
			delete os;
		}
	}

	void flush(){
		dump();
		data.clear();
	}
	
	void set_max_mem(float max_size){
		this->max_size = max_size;
	}

	static SparseMatrixStream fromFile(string graph_file_name){
		ifstream in(graph_file_name, ios::in | ios::binary);
		if(!in){
			throw runtime_error("Cannot read the graph file");
		}
		size_t n;
		in.read((char*) &n, sizeof(size_t));
		uint32_t indexversion;
		in.read((char*) &indexversion, sizeof(uint32_t));
		if(indexversion != 0){
			throw runtime_error("This file cannot be read");
		}
		SparseMatrixStream sms(n);
		size_t unit_size = 2*sizeof(uint32_t)+sizeof(double);
		unsigned long long buffer_size = (5ULL*1024*1024 / unit_size) * unit_size;
		char* buffer = new char[buffer_size];
		while(in.good()){
			uint32_t first_component;
			in.read((char*) &first_component, sizeof(uint32_t));
			uint32_t size;
			in.read((char*) &size, sizeof(uint32_t));
			const unsigned long long block_size = size*unit_size;

			unsigned long long bytes_read = 0;
			for(uint32_t ichunk = 0; ichunk < ceil(block_size/(1.0 * buffer_size)); ichunk++){
				const unsigned long long bytes = min(buffer_size - (buffer_size % unit_size), block_size - bytes_read);
				in.read(buffer, bytes);
				sms.build_graph(buffer, bytes);
				bytes_read += bytes;
			}
		}
		in.close();
		delete[] buffer;
		return sms;
	}

	static vector<vector<Eigen::Triplet<T>>> collect_components(vector<vector<uint32_t>*>* indices, string graph_file_name){
		ifstream in(graph_file_name, ios::in | ios::binary);
		if(!in){
			throw runtime_error("Cannot read the graph file");
		}
		size_t n;
		in.read((char*) &n, sizeof(size_t));
		uint32_t indexversion;
		in.read((char*) &indexversion, sizeof(uint32_t));
		if(indexversion != 0){
			throw runtime_error("This file cannot be read");
		}

		unordered_set<uint32_t> set;
		for(const vector<uint32_t>* const idxs : *indices){
			set.insert(idxs->begin(), idxs->end());
		}

		SparseMatrixStream sms(&set);
		size_t unit_size = 2*sizeof(uint32_t)+sizeof(double);
		// This is per thread, so only 5MB buffer allowed 
		unsigned long long buffer_size = 5ULL*1024*1024;
		char* buffer = new char[buffer_size];
		while(in.good()){
			uint32_t first_component;
			in.read((char*) &first_component, sizeof(uint32_t));
			uint32_t size;
			in.read((char*) &size, sizeof(uint32_t));
			const unsigned long long block_size = size*unit_size;

			if(set.count(first_component) == 1){
				unsigned long long bytes_read = 0;
				for(uint32_t ichunk = 0; ichunk < ceil(block_size/(1.0 * buffer_size)); ichunk++){
					const unsigned long long bytes = min(buffer_size - (buffer_size % unit_size), block_size - bytes_read);
					in.read(buffer, bytes);
					sms.consume(buffer, bytes);
					bytes_read += bytes;
				}
			}
			else{
				in.ignore(block_size);
			}
		}
		in.close();
		delete[] buffer;
		return sms.getComponents(indices);
	}

	vector<vector<uint32_t>> get_indices(){
		vector<unordered_set<uint32_t>> sets = disjointSet->getListOfSets();
		vector<vector<uint32_t>> indices;
		for(uint32_t iset = 0; iset < sets.size(); iset++){
			indices.emplace_back(sets[iset].begin(), sets[iset].end());
		}
		return indices;
	}

	Eigen::SparseMatrix<T> getMatrix(){
		Eigen::SparseMatrix<T> m(n,n);
		m.setFromTriplets(data.begin(), data.end());
		return m;
	}
	uint64_t getNumberOfElements(){
		return data.size();
	}
};
}}

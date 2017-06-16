// Author: bdgoocci@gmail.com
// Description:  
//  assume 2 threads,
//  one thread called A is responsible for writing into the memory, 
//  the other thread called B reads the content which is written by thread A at the same time.
//  Semaphore is needed because there is a competition between threads.
//  If thread A and thread B are C++ threads, shared_memory.cpp is suitable.
//  If python thread is needed, please refer to shared_memory.py
#include "shared_memory.hpp"
#include "base/log.hpp"

namespace Husky {

	SharedMemory::SharedMemory(int bf_size, std::string shm_name, std::string sem_post_name, std::string sem_wait_name)
		: shm_obj(boost::interprocess::open_or_create, shm_name.c_str(), boost::interprocess::read_write),
		sem_post(boost::interprocess::open_or_create_t(), sem_post_name.c_str(), 0),
		sem_wait(boost::interprocess::open_or_create_t(), sem_wait_name.c_str(), 0) {
	    	buffer_size = bf_size - 8;
    		shm_obj.truncate(bf_size);
		region = boost::interprocess::mapped_region(shm_obj, boost::interprocess::read_write);
    		this->shm_name = shm_name;
    		this->sem_post_name = sem_post_name;
    		this->sem_wait_name = sem_wait_name;
    		region_start = reinterpret_cast<char*>(region.get_address());
    		buffer_w_ptr = reinterpret_cast<int*>(region_start + bf_size - 8);
    		buffer_r_ptr = reinterpret_cast<int*>(region_start + bf_size - 4);
    		*buffer_w_ptr = 0;
    		*buffer_r_ptr = 0;
            sem_post.post();
            sem_wait.wait();
	}

	SharedMemory::~SharedMemory() {
		boost::interprocess::shared_memory_object::remove(shm_name.c_str());
		boost::interprocess::named_semaphore::remove(sem_post_name.c_str());
		boost::interprocess::named_semaphore::remove(sem_wait_name.c_str());
	}

    void SharedMemory::write(std::string str) {  
        BinStream bin;
        bin.push_back_bytes(str.c_str(), str.size());
        write(bin);
    }

	void SharedMemory::write(BinStream& binstream) {
        // write params recording progress
        int writing_bytes_progress = 0;
        
        int buffer_available;
        while(true) {
            buffer_available = count_buffer_available();
            if (buffer_available < 4) {
                sem_wait.wait();
            } else {
                write_binstream_size(binstream.size());
                // log_msg("counter value before post writing size: " + std::to_string(sem_post.get_value()));
                sem_post.try_wait();
                sem_post.post();
                // log_msg("counter value after post writing size: " + std::to_string(sem_post.get_value()));
                break;
            }
        }
		
        while(writing_bytes_progress < binstream.size()) {
			// count buffer avaiable
            buffer_available = count_buffer_available();
			if (buffer_available == 0) {
				sem_wait.wait();
				continue;
			}

			writing_bytes_progress += write_binstream(binstream, buffer_available, writing_bytes_progress);
                                                
            // log_msg("counter value before post: " + std::to_string(sem_post.get_value()));
			sem_post.try_wait();
			sem_post.post();
            // log_msg("counter value after post: " + std::to_string(sem_post.get_value()));
		}

	}

	void SharedMemory::write_binstream_size(int size) {
		roll_write(reinterpret_cast<char*>(&size), 4);
	}

	int SharedMemory::write_binstream(BinStream& binstr, int buffer_avail, int progress) {
		if (buffer_avail >= binstr.size() - progress) {
			roll_write(binstr.get_remained_buffer() + progress, static_cast<int>(binstr.size()) - progress);
			return static_cast<int>(binstr.size()) - progress;
		} else {
			roll_write(binstr.get_remained_buffer() + progress, buffer_avail);
			return buffer_avail;
		}
	}

	int SharedMemory::count_buffer_available() {
        assert(*buffer_w_ptr >= 0);
        assert(*buffer_w_ptr < buffer_size);
        assert(*buffer_r_ptr >= 0);
        assert(*buffer_r_ptr < buffer_size);
		if (*buffer_w_ptr == *buffer_r_ptr) {
			return buffer_size - 1;
		} else if (*buffer_w_ptr > *buffer_r_ptr) {
			return buffer_size - *buffer_w_ptr + *buffer_r_ptr - 1;
		} else {
			return *buffer_r_ptr - *buffer_w_ptr - 1;
		}
	}

    int SharedMemory::count_right_len() {
        int buffer_w_ptr_val = *buffer_w_ptr;
        int buffer_r_ptr_val = *buffer_r_ptr;
        assert(*buffer_w_ptr >= 0);
        assert(*buffer_w_ptr < buffer_size);
        assert(*buffer_r_ptr >= 0);
        assert(*buffer_r_ptr < buffer_size);

        if (buffer_r_ptr_val == 0) {
            return buffer_size - buffer_w_ptr_val - 1;
        } else if (buffer_r_ptr_val <= buffer_w_ptr_val) {
            return buffer_size - buffer_w_ptr_val;
        } else {
            return buffer_r_ptr_val - buffer_w_ptr_val - 1;
        }
    }

	void SharedMemory::roll_write(const char* bin, int size) {
		int right_len = count_right_len();

        assert(*buffer_w_ptr >= 0);
        assert(*buffer_w_ptr < buffer_size);
        assert(*buffer_r_ptr >= 0);
        assert(*buffer_r_ptr < buffer_size);

		if (right_len >= size) {
			memcpy(region_start + *buffer_w_ptr, bin, size);
            assert(*buffer_w_ptr <= buffer_size);
            if (*buffer_w_ptr + size >= buffer_size) {
                assert(0 != *buffer_r_ptr);
                *buffer_w_ptr = 0;
            } else {
                assert(*buffer_w_ptr + size != *buffer_r_ptr);
                *buffer_w_ptr += size;
            }
		} else {
			memcpy(region_start + *buffer_w_ptr, bin, right_len);
			memcpy(region_start, bin + right_len, size - right_len);
			*buffer_w_ptr = size - right_len;
		}
               
        assert(*buffer_w_ptr >= 0);
        assert(*buffer_w_ptr < buffer_size);
        assert(*buffer_r_ptr >= 0);
        assert(*buffer_r_ptr < buffer_size);
	}

    std::string SharedMemory::read_string() {
        return read().to_string();
    }

	BinStream SharedMemory::read() {
		BinStream msg;
		BinStream msg_len_bin;
		int msg_len;
		int read_len = 0;
                
        while (true) {
            roll_read(msg_len_bin, 4 - msg_len_bin.size());
            sem_post.try_wait();
            sem_post.post();
            if (msg_len_bin.size() < 4) {
                sem_wait.post();    
            } else {
                break;
            }
        }
		msg_len = *reinterpret_cast<int*>(msg_len_bin.get_buffer());

		while(msg.size() < msg_len) {
			roll_read(msg, msg_len - msg.size());
            sem_post.try_wait();
			sem_post.post();
			if (msg.size() < msg_len) {
				sem_wait.wait();
			}
		}
		return msg;
	}

	void SharedMemory::roll_read(BinStream& bin, int size) {
		int r_pos = *buffer_r_ptr;
		int w_pos = *buffer_w_ptr;

        assert(*buffer_w_ptr >= 0);
        assert(*buffer_w_ptr < buffer_size);
        assert(*buffer_r_ptr >= 0);
        assert(*buffer_r_ptr < buffer_size);
		
        if (r_pos < w_pos) {
			int max_read = w_pos - r_pos;
			if (max_read < size) {
				bin.push_back_bytes(region_start + r_pos, max_read);
				*buffer_r_ptr = r_pos + max_read;
			} else {
				bin.push_back_bytes(region_start + r_pos, size);
				*buffer_r_ptr = r_pos + size;
			}
		} else if (r_pos > w_pos) {
			int max_right_read = buffer_size - r_pos;
			int max_left_read = w_pos;

			if (size > max_right_read) {
				// return right_read totally
				bin.push_back_bytes(region_start + r_pos, max_right_read);
				int surplus = size - max_right_read;

				if (surplus > max_left_read) {
					bin.push_back_bytes(region_start, max_left_read);
					*buffer_r_ptr = max_left_read;
				} else {
					bin.push_back_bytes(region_start, surplus);
					*buffer_r_ptr = surplus;
				}
			} else {
				bin.push_back_bytes(region_start + r_pos, size);
                if (r_pos + size >= buffer_size) {
                    *buffer_r_ptr = 0;
                } else {
				    *buffer_r_ptr = r_pos + size;
                }
			}
		}

        assert(*buffer_w_ptr >= 0);
        assert(*buffer_w_ptr < buffer_size);
        assert(*buffer_r_ptr >= 0);
        assert(*buffer_r_ptr < buffer_size);
	}	
}

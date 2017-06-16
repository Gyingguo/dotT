# Author: bdgoocci@gmail.com
# Description: refer to shared_memory.cpp 
import posix_ipc
import mmap
import os
import struct
import sys

class SharedMemory:

	int_mode = struct.Struct('i')
	
	def __init__(self, bf_size, shm_name, sem_post_name, sem_wait_name):
                self.buffer_size = bf_size - 8
		self.sem_post = posix_ipc.Semaphore(sem_post_name, posix_ipc.O_CREAT)
		self.sem_post_name = sem_post_name
		self.sem_wait = posix_ipc.Semaphore(sem_wait_name, posix_ipc.O_CREAT)
		self.sem_wait_name = sem_wait_name
                self.sem_wait.acquire()
		self.shm_obj = mmap.mmap(os.open(shm_name, os.O_RDWR), bf_size)
		self.shm_name = shm_name
		self.region_start = 0
		self.buffer_w_ptr = bf_size - 8
		self.buffer_r_ptr = bf_size - 4
                self.sem_post.release()

	def _def_(self):
		# override
		self.shm_obj.close()
		self.sem_post_name.close()
		self.sem_wait_name.close()

	def set_buffer_w_ptr(self, w_ptr_val):
		self.shm_obj.seek(self.buffer_w_ptr)
                if w_ptr_val >= self.buffer_size:
                    self.shm_obj.write(self.int_mode.pack(0))
                else:
                    self.shm_obj.write(self.int_mode.pack(w_ptr_val))

	def get_buffer_w_ptr(self):
		self.shm_obj.seek(self.buffer_w_ptr)
		return self.int_mode.unpack(self.shm_obj.read(4))[0]

	def set_buffer_r_ptr(self, r_ptr_val):
		self.shm_obj.seek(self.buffer_r_ptr)
                if r_ptr_val >= self.buffer_size:
                    self.shm_obj.write(self.int_mode.pack(0))
                else:
		    self.shm_obj.write(self.int_mode.pack(r_ptr_val))

	def get_buffer_r_ptr(self):
		self.shm_obj.seek(self.buffer_r_ptr)
		return self.int_mode.unpack(self.shm_obj.read(4))[0]

	def write(self, binstream):
		writing_bytes_progress = 0;
                
               # pdb.set_trace()
               
                while 1:
		        buffer_available = self.count_buffer_available()

		        if buffer_available < 4:
		 		self.sem_wait.acquire()
		 	else:
		 		self.write_binstream_size(len(binstream))
                                self.sem_post.acquire(0)
                                self.sem_post.release()
                                break

		while writing_bytes_progress < len(binstream):
		 	 buffer_available = self.count_buffer_available()
                         if buffer_available == 0:
		 	 	self.sem_wait.acquire()
		 	 	continue

		 	 writing_bytes_progress += self.write_binstream(binstream, buffer_available, writing_bytes_progress)

		 	 self.sem_post.acquire(0)
		 	 self.sem_post.release()


	def write_binstream_size(self, size):
		self.roll_write(self.int_mode.pack(size), 4)

	def write_binstream(self, binstr, buffer_avail, progress):
		if buffer_avail >= len(binstr) - progress:
			self.roll_write(binstr[progress:], len(binstr) - progress)
			return len(binstr) - progress
		else:
			self.roll_write(binstr[progress:], buffer_avail)
			return buffer_avail

	def count_buffer_available(self):
		buffer_w_ptr_val = self.get_buffer_w_ptr()
		buffer_r_ptr_val = self.get_buffer_r_ptr()
                
		if buffer_w_ptr_val == buffer_r_ptr_val:
			return self.buffer_size - 1
		elif buffer_w_ptr_val > buffer_r_ptr_val:
			return self.buffer_size - buffer_w_ptr_val + buffer_r_ptr_val - 1
		else:
			return buffer_r_ptr_val - buffer_w_ptr_val - 1

	def roll_write(self, bin, size):
               # pdb.set_trace()
		buffer_w_ptr_val = self.get_buffer_w_ptr()

		right_len = self.count_right_len()
                
		if right_len >= size:
			self.shm_obj.seek(buffer_w_ptr_val)
                        self.shm_obj.write(bin[:size])
                        self.set_buffer_w_ptr(buffer_w_ptr_val + size)
		else:
			self.shm_obj.seek(buffer_w_ptr_val)
			self.shm_obj.write(bin[:right_len])
			self.shm_obj.seek(0)
			self.shm_obj.write(bin[right_len:size])
                        self.set_buffer_w_ptr(size - right_len)

        
        def count_right_len(self):
                buffer_w_ptr_val = self.get_buffer_w_ptr()
                buffer_r_ptr_val = self.get_buffer_r_ptr()

                if buffer_r_ptr_val == 0:
                    return self.buffer_size - buffer_w_ptr_val - 1
                elif buffer_r_ptr_val <= buffer_w_ptr_val:
                    return self.buffer_size - buffer_w_ptr_val
                else:
                    return buffer_r_ptr_val - buffer_w_ptr_val - 1


        def read(self):
		msg = ''
                msg_len_bin = ''
               
                while 1:
		    msg_len_bin += self.roll_read(4 - len(msg_len_bin)) 
                    self.sem_post.acquire(0)
                    self.sem_post.release()
                    if len(msg_len_bin) < 4:
                        self.sem_wait.acquire()
                    else:
                        break
                
                #pdb.set_trace()
                msg_len = self.int_mode.unpack(msg_len_bin)[0]
                
		while len(msg) < msg_len:
                        msg += self.roll_read(msg_len - len(msg)) 
                 	self.sem_post.acquire(0)
			self.sem_post.release()

			if len(msg) < msg_len:
                            self.sem_wait.acquire()

                return msg


	def roll_read(self, size):
		r_ptr_val = self.get_buffer_r_ptr()
		w_ptr_val = self.get_buffer_w_ptr()
                bin = ''

                if r_ptr_val < w_ptr_val:
			max_read = w_ptr_val - r_ptr_val

			if max_read < size:
				self.shm_obj.seek(self.region_start + r_ptr_val)
				bin = self.shm_obj.read(max_read)
				self.set_buffer_r_ptr(r_ptr_val + max_read)
			else:
				self.shm_obj.seek(self.region_start + r_ptr_val)
				bin = self.shm_obj.read(size)         
				self.set_buffer_r_ptr(r_ptr_val + size)

		elif r_ptr_val > w_ptr_val:
			max_right_read = self.buffer_size - r_ptr_val
			max_left_read = w_ptr_val

			if size > max_right_read:
				self.shm_obj.seek(self.region_start + r_ptr_val)
				bin = self.shm_obj.read(max_right_read)
				surplus = size - max_right_read

				if surplus > max_left_read:
					self.shm_obj.seek(self.region_start)
					bin += self.shm_obj.read(max_left_read)
					self.set_buffer_r_ptr(max_left_read)
				else:
					self.shm_obj.seek(self.region_start)
					bin += self.shm_obj.read(surplus)
					self.set_buffer_r_ptr(surplus)

			else:
				self.shm_obj.seek(self.region_start + r_ptr_val)
				bin += self.shm_obj.read(size)
    				self.set_buffer_r_ptr(r_ptr_val + size)

		return bin 



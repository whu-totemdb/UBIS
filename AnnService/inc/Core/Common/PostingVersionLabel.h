// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef _SPTAG_COMMON_POSTINGVERSIONLABEL_H_
#define _SPTAG_COMMON_POSTINGVERSIONLABEL_H_

#include <atomic>
#include "Dataset.h"
#include <tbb/concurrent_hash_map.h>
#include "inc/Core/Common/FineGrainedLock.h"
#include <thread>
#include <chrono>

namespace SPTAG
{
    namespace COMMON
    {
        enum class status : std::uint64_t{
            normal = 0x00,
            splitting = 0x01,
            merging = 0x02,
            deleted = 0x03
        };

        class PostingVersionLabel
        {
        private:
            std::atomic<SizeType> m_deleted;//32 bits atomic signed integer
            Dataset<std::uint64_t> m_data;//uint64_t is 8 bytes
            COMMON::FineGrainedRWLock m_countLocks;
            tbb::concurrent_hash_map<SizeType, std::string> m_vectorCache;

            //the lowest bit of each postion
            const int status_offset = 0;
            const int count_offset = 2;
            const int PID2_offset = 18;
            const int PID1_offset = 41;
            

            //(1ULL << x) - 1 equals a 64-bit binary whose the lowest x bits are 1 and other bits are 0;
            const std::uint64_t status_range = ((1ULL << count_offset) - 1) ^ ((1ULL << status_offset) - 1);//[1:0] is valid, 00 is normal,  01 is splitting, 10 is merging, 11 is deleted
            const std::uint64_t count_range = ((1ULL << PID2_offset) - 1) ^ ((1ULL << count_offset) - 1);//[17:2] is valid
            const std::uint64_t PID2_range = ((1ULL << PID1_offset) - 1) ^ ((1ULL << PID2_offset) - 1);//[40:18] is valid
            const std::uint64_t PID1_range = (~(0ULL)) ^ ((1ULL << PID1_offset) - 1);//[63:41] is valid

            
            
        public:
            PostingVersionLabel() 
            {
                m_deleted = 0;
                m_data.SetName("postingVersionLabelID");
            }

            void Initialize(SizeType size, SizeType blockSize, SizeType capacity)
            {
                //the function will set each posting version label to the max value of uint64_t
                m_data.Initialize(size, 1, blockSize, capacity);
                
                //pid1, pid2 are all consist of 1
                //set each posting status to normal and count to 0
                for (SizeType i = 0; i < size; i++)
                {
                    SetDefault(i);
                }
                
            }

            inline size_t Count() const { return m_data.R() - m_deleted.load(); }

            inline size_t GetDeleteCount() const { return m_deleted.load();}

            inline bool IsNormal(const SizeType& key) const
            {
                return (*m_data[key] & status_range) == static_cast<uint64_t>(status::normal);
            }

            inline bool SetNormal(const SizeType& key)
            {
                uint64_t oldValue = *m_data[key];
                uint64_t oldValidValue = oldValue & (~status_range);
                uint64_t newValue = oldValidValue | static_cast<uint64_t>(status::normal);
                uint64_t v = (uint64_t)InterlockedExchange8((char*)(m_data[key]), (char)newValue);
                if (oldValue == v) return false;
                return true;
            }

            inline bool Merging(const SizeType& key) const
            {
                return (*m_data[key] & status_range) == static_cast<uint64_t>(status::merging);
            }

            inline bool Merge(const SizeType& key)
            {
                uint64_t oldValue = *m_data[key];
                uint64_t oldValidValue = oldValue & (~status_range);
                uint64_t newValue = oldValidValue | static_cast<uint64_t>(status::merging);
                uint64_t v = (uint64_t)InterlockedExchange8((char*)(m_data[key]), (char)newValue);
                if (oldValue == v) return false;
                return true;
            }

            inline bool Splitting(const SizeType& key) const
            {
                return (*m_data[key] & status_range) == static_cast<uint64_t>(status::splitting);
            }

            inline bool Split(const SizeType& key)
            {
                uint64_t oldValue = *m_data[key];
                uint64_t oldValidValue = oldValue & (~status_range);
                uint64_t newValue = oldValidValue | static_cast<uint64_t>(status::splitting);
                uint64_t v = (uint64_t)InterlockedExchange8((char*)(m_data[key]), (char)newValue);
                if (oldValue == v) return false;
                return true;
            }

            inline bool Deleted(const SizeType& key) const
            {
                return (*m_data[key] & status_range) == static_cast<uint64_t>(status::deleted);
            }

            inline bool Delete(const SizeType& key)
            {
                uint64_t oldValue = *m_data[key];
                uint64_t oldValidValue = oldValue & (~status_range);
                uint64_t newValue = oldValidValue | static_cast<uint64_t>(status::deleted);
                uint64_t v = (uint64_t)InterlockedExchange8((char*)(m_data[key]), (char)newValue);
                if (oldValue == v) return false;
                m_deleted++;
                return true;
            }

            inline uint64_t GetData(const SizeType& key)
            {
                return *m_data[key];
            }

            inline /*uint64_t*/void addToVectorCache(SizeType& postingID, int vector_num, std::string& newVectorInfo){
                //std::unique_lock<std::shared_timed_mutex> lock(m_countLocks[postingID]);

                tbb::concurrent_hash_map<SizeType, std::string>::accessor postingIDAccessor;
                if (m_vectorCache.find(postingIDAccessor, postingID)) {
                    std::string oldCacheInfo = postingIDAccessor->second;
                    (postingIDAccessor->second).append(newVectorInfo);
                }
                else{
                    tbb::concurrent_hash_map<SizeType, std::string>::value_type newPair(postingID, newVectorInfo);
                    m_vectorCache.insert(newPair);
                }

                /*uint64_t oldCount, oldValue;

                do{                                                           
                    oldValue = *m_data[postingID];
                    oldCount = (oldValue & count_range) >> count_offset;

                    if(oldCount + vector_num <= (count_range >> count_offset)) {
                        break;
                    }
                    lock.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    lock.lock();
                }while(true);

                
                *m_data[postingID] = ((oldCount + vector_num) << count_offset) | (oldValue & (~count_range));

                return (oldCount + vector_num);*/
            }

            inline std::string eraseVectorCacheInfo(SizeType& postingID, uint64_t* final_result, int m_vectorInfoSize){
                std::string str("");
                tbb::concurrent_hash_map<SizeType, std::string>::const_accessor postingIDAccessor;
                //std::unique_lock<std::shared_timed_mutex> lock(m_countLocks[postingID]);

                if (m_vectorCache.find(postingIDAccessor, postingID)) {

                    //std::unique_lock<std::shared_timed_mutex> lock(m_countLocks[postingID]);//deadlock!

                    str = postingIDAccessor->second;  
                    m_vectorCache.erase(postingIDAccessor);
                
                    /*uint64_t vec_num = reinterpret_cast<uint64_t>(str.size() / m_vectorInfoSize);
                    uint64_t oldValue = *m_data[postingID];
                    uint64_t oldCount = (oldValue & count_range) >> count_offset;

                    *final_result = (oldCount >= vec_num) ? (oldCount - vec_num) : 0;

                    *m_data[postingID] = (*final_result << count_offset) | (oldValue & (~count_range));*/
                }
                return str;
            }

            inline std::string getVectorCacheInfo(SizeType& postingID){
                std::string str("");
                tbb::concurrent_hash_map<SizeType, std::string>::const_accessor postingIDAccessor;

                if (m_vectorCache.find(postingIDAccessor, postingID)) {
                    str = postingIDAccessor->second;     
                }
                return str;
            }

            inline uint64_t SetDefault(SizeType& postingID){
                std::unique_lock<std::shared_timed_mutex> lock(m_countLocks[postingID]);
                uint64_t oldValue = *m_data[postingID];
                uint64_t newValue = (~(0ULL)) << PID2_offset;

                *m_data[postingID] = newValue;

                return oldValue;
            }

            inline uint64_t GetCount(const SizeType& key)
            {
                std::unique_lock<std::shared_timed_mutex> lock(m_countLocks[key]);
                return (*m_data[key] & count_range) >> count_offset;
            }

            inline uint64_t ClearCurrentCount(SizeType& key)
            {
                std::unique_lock<std::shared_timed_mutex> lock(m_countLocks[key]);
                
                uint64_t oldValue = *m_data[key];
                uint64_t oldCount = (*m_data[key] & count_range) >> count_offset;
                
                uint64_t newValue = 0ULL | (oldValue & (~count_range));
                *m_data[key] = newValue;

                return oldCount;                             
            }

            inline uint64_t IncCount(SizeType& key, uint64_t* newCount, int num)
            {
                uint64_t oldCount, oldValue;
                std::unique_lock<std::shared_timed_mutex> lock(m_countLocks[key]);
                do{                                                           
                    oldValue = *m_data[key];
                    oldCount = (oldValue & count_range) >> count_offset;

                    if(oldCount + num <= (count_range >> count_offset)) {
                        break;
                    }
                    lock.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    lock.lock();
                }while(true);
                
                *newCount = (oldCount + num) << count_offset;

                uint64_t newValue = *newCount | (oldValue & (~count_range)); 
                *m_data[key] = newValue;

                return oldCount;  
                /*while (true) {
                    if (Deleted(key)) return false;
                    uint64_t oldValue = *m_data[key];
                    uint64_t oldCount = (*m_data[key] & count_range) >> count_offset;
                    *newCount = (oldCount + num) << count_offset;

                    uint64_t newValue = *newCount | (oldValue & (~count_range));                   
                    if (((uint64_t)InterlockedCompareExchange((char*)m_data[key], (char)newValue, (char)oldValue)) == oldValue) {
                        return true;
                    }
                }*/
            }

            inline SizeType GetPID1(const SizeType& key){
                uint64_t PID1_64 = *m_data[key] & PID1_range;
                return static_cast<SizeType>(PID1_64 >> PID1_offset);
            }

            inline void SetPID1(const SizeType& key, SizeType p_id){
                uint64_t value = static_cast<uint64_t>(p_id) << PID1_offset;
                uint64_t orginal_posting_data = *m_data[key];

                *m_data[key] = value | (orginal_posting_data & (~PID1_range));

                //uint64_t new_posting_data = *m_data[key];
            }

            inline SizeType GetPID2(const SizeType& key){
                uint64_t PID2_64 = *m_data[key] & PID2_range;
                return static_cast<SizeType>(PID2_64 >> PID2_offset);
            }

            inline void SetPID2(const SizeType& key, SizeType p_id){
                uint64_t value = static_cast<uint64_t>(p_id) << PID2_offset;
                uint64_t orginal_posting_data = *m_data[key];

                *m_data[key] = value | (orginal_posting_data & (~PID2_range));

                //uint64_t new_posting_data = *m_data[key];
            }

            inline void SetCount(const SizeType& key, SizeType count){
                uint64_t value = static_cast<uint64_t>(count) << count_offset;
                uint64_t orginal_posting_data = *m_data[key];

                *m_data[key] = value | (orginal_posting_data & (~count_range));
            }

            inline SizeType GetPostingNum()
            {
                return m_data.R();
            }

            inline SizeType GetvValidPostingNum()
            {
                return m_data.R() - m_deleted.load();
            }

            inline ErrorCode Save(std::shared_ptr<Helper::DiskIO> output)
            {
                SizeType deleted = m_deleted.load();
                IOBINARY(output, WriteBinary, sizeof(SizeType), (char*)&deleted);
                return m_data.Save(output);
            }

            inline ErrorCode Save(const std::string& filename)
            {
                LOG(Helper::LogLevel::LL_Info, "Save %s To %s\n", m_data.Name().c_str(), filename.c_str());
                auto ptr = f_createIO();
                if (ptr == nullptr || !ptr->Initialize(filename.c_str(), std::ios::binary | std::ios::out)) return ErrorCode::FailedCreateFile;
                return Save(ptr);
            }

            inline ErrorCode Load(std::shared_ptr<Helper::DiskIO> input, SizeType blockSize, SizeType capacity)
            {
                SizeType deleted;
                IOBINARY(input, ReadBinary, sizeof(SizeType), (char*)&deleted);
                m_deleted = deleted;
                return m_data.Load(input, blockSize, capacity);
            }

            inline ErrorCode Load(const std::string& filename, SizeType blockSize, SizeType capacity)
            {
                LOG(Helper::LogLevel::LL_Info, "Load %s From %s\n", m_data.Name().c_str(), filename.c_str());
                auto ptr = f_createIO();
                if (ptr == nullptr || !ptr->Initialize(filename.c_str(), std::ios::binary | std::ios::in)) return ErrorCode::FailedOpenFile;
                return Load(ptr, blockSize, capacity);
            }

            inline ErrorCode Load(char* pmemoryFile, SizeType blockSize, SizeType capacity)
            {
                m_deleted = *((SizeType*)pmemoryFile);
                return m_data.Load(pmemoryFile + sizeof(SizeType), blockSize, capacity);
            }

            inline ErrorCode AddBatch(SizeType postingID, SizeType num)
            {
                //std::unique_lock<std::shared_timed_mutex> lock(m_countLocks[postingID]);
                ErrorCode e = m_data.AddBatch(num);
                
                //uint64_t oldValue = *m_data[postingID];
                uint64_t newValue = (~(0ULL)) << PID2_offset;

                *m_data[postingID] = newValue;

                return e;
            }

            inline std::uint64_t BufferSize() const 
            {
                return m_data.BufferSize() + sizeof(SizeType);
            }

            inline void SetR(SizeType num)
            {
                m_data.SetR(num);
            }
        };
    }
}

#endif // _SPTAG_COMMON_LABELSET_H_

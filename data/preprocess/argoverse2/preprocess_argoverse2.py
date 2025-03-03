import pandas as pd
import numpy as np
import os  
import psutil
import struct
import math
import torch
import torch.distributions as dist

import Transformer_Embedding

base_data_max_number = 2000000
query_data_max_number = 25000
vector_embedding_dim = 256
min_vector_len = 45

base_data = np.zeros((base_data_max_number, 110, 2))  
query_data = np.zeros((query_data_max_number, 110, 2))
data_sum = 0
query_data_sum = 0
base_data_timestamps = {}
query_data_timestamps = {}



data_count = 0
query_data_count = 0


ride = 100
K = 10


def parquet2csv(base_file_path):   
    paths = os.walk(base_file_path)
    

    for path, dir_lst, file_lst in paths:
        dir_path = ''
        for dir_name in dir_lst:
            dir_path = os.path.join(path, dir_name)
            files= os.listdir(dir_path)
            dir_path = dir_path+'/'
            print(dir_path)

            for file in files:
                if(file.split('.')[1] == 'parquet'):
                    print(os.path.join(dir_path, file))
                    
                    df = pd.read_parquet(os.path.join(dir_path, file))

                    
                    df.to_csv(os.path.join(dir_path, file.split('.')[0]+'.csv'), index=False)

def L2dis(base_embedding, query_embedding):
    if(len(base_embedding) != len(query_embedding)):
        return -1.0
    
    sum = 0.0
    for i in range(len(base_embedding)):
        sum += math.pow(abs(base_embedding[i] - query_embedding[i]), 2)

    return math.sqrt(sum)

def genereteTruthSplittedByRide(base_embeddings, query_embeddings, query_vector_range, ride, K):
    i = 0
    old_i = 0
    truthPath = 'embeddings/test/truth/truth_embeddings_'

    split = -1
    
    query_truth_top_k_num = np.zeros((len(query_embeddings)), dtype=int)
    query_truth_top_k_dis = np.zeros((len(query_embeddings), K))
    query_truth_top_k_index = np.zeros((len(query_embeddings), K), dtype=int)
    query_truth_top_k_max = np.zeros((len(query_embeddings)))

    while i < len(query_embeddings):
        
        old_split = split
        split = int(query_vector_range[i])
        
        sub_base_embeddings = base_embeddings[0:split+1]

        
        for m in range(0,old_i):

            for j in range(old_split+1, split+1):
                dis = L2dis(sub_base_embeddings[j], query_embeddings[m])
                current_num = query_truth_top_k_num[m]
                distances = query_truth_top_k_dis[m]
                max_v = query_truth_top_k_max[m]

                if current_num < K:
                    query_truth_top_k_index[m][current_num] = j
                    query_truth_top_k_dis[m][current_num] = dis
                    query_truth_top_k_num[m] = current_num + 1
                    query_truth_top_k_max[m] = max(max_v, dis)
                    
                else:
                    
                    if dis < max_v :
                        
                        max_ = 0
                        index = 0
                        c = 0
                        for n in range(len(distances)):
                            if distances[n] == max_v and c == 0:
                                index = n
                                c = 1
                                continue

                            if max_ < distances[n]:
                                max_ = distances[n]
                        
                        query_truth_top_k_index[m][index] = j
                        query_truth_top_k_dis[m][index] = dis
                        query_truth_top_k_max[m] = max(max_, dis)

                        
        for m in range(old_i, i+1):
            for j in range(0, split+1):
                dis = L2dis(sub_base_embeddings[j], query_embeddings[m])
                current_num = query_truth_top_k_num[m]
                distances = query_truth_top_k_dis[m]
                max_v = query_truth_top_k_max[m]

                if current_num < K:
                    query_truth_top_k_index[m][current_num] = j
                    query_truth_top_k_dis[m][current_num] = dis
                    query_truth_top_k_num[m] = current_num + 1
                    query_truth_top_k_max[m] = max(max_v, dis)
                    
                else:
                    if dis < max_v :
                        
                        max_ = 0                       
                        index = 0
                        c = 0
                        for n in range(len(distances)):
                            if distances[n] == max_v and c == 0:
                                index = n
                                c = 1
                                continue

                            if max_ < distances[n]:
                                max_ = distances[n]
                                
                        
                        query_truth_top_k_index[m][index] = j
                        query_truth_top_k_dis[m][index] = dis
                        query_truth_top_k_max[m] = max(max_, dis)



        with open(truthPath+str(i), 'wb') as file:
        
            file.write((i+1).to_bytes(4, byteorder='little', signed= False))
            file.write(K.to_bytes(4, byteorder='little', signed= False))

            for x in range(0, i+1):
                print(query_truth_top_k_index[x], query_truth_top_k_dis[x])
                for y in range(len(query_truth_top_k_index[x])):
                    file.write(query_truth_top_k_index[x][y].item().to_bytes(4, byteorder='little', signed= False))

        old_i = i
        i = i + ride



def read_vector_range_bin_file(file_path):
    with open(file_path, "rb") as f:
        row_bin = f.read(4)
        assert row_bin != b''
        row, = struct.unpack('i', row_bin)
        
        i = 0
        ranges = np.zeros((row))
        while 1:

            vec, = struct.unpack('i' , f.read(4))
                   
            ranges[i] = vec
            i += 1
            if i == row:
                break

        return ranges


def read_vector_bin_file(file_path):
    with open(file_path, "rb") as f:
        row_bin = f.read(4)
        assert row_bin != b''
        row, = struct.unpack('i', row_bin)

        dim_bin = f.read(4)
        assert dim_bin != b''
        dim, = struct.unpack('i', dim_bin)

        i = 0
        vector_embeddings = np.zeros((row, dim));
        while 1:


            for j in range(dim):
                vec, = struct.unpack('f', f.read(4))
                vector_embeddings[i][j] = vec

            i += 1
            if i == row:
                break
        
        return vector_embeddings 


def write_bin_file(file_path, data_array, num):
    with open(file_path, 'wb') as file:
        
        file.write(num.to_bytes(4, byteorder='little', signed= False))
        file.write(vector_embedding_dim.to_bytes(4, byteorder='little', signed= False))
    
        for i in range(num):
            for j in range(len(data_array[i])):
                file.write(struct.pack('f', data_array[i][j]))


def read_csv(base_file_path):  
    global data_sum, query_data_sum, query_data, base_data, data_count, query_data_count, ride, K

    paths = os.walk(base_file_path)
    num = 0
    limit_num = query_data_max_number

    for path, dir_lst, file_lst in paths:
        dir_path = ''
        old_base_data_count = 0
        for dir_name in dir_lst:
            dir_path = os.path.join(path, dir_name)
            files= os.listdir(dir_path)
            dir_path = dir_path+'/'
            #print(dir_path)

            
            for file in files:
                if(file.split('.')[1] == 'csv'):
                    print(os.path.join(dir_path, file))
                    read_csv_location_data(os.path.join(dir_path, file))
                    num+=1
                    print(num)

        

    
    print(data_sum, query_data_sum)

    
    base_data_timestamps_order = sorted(base_data_timestamps.items(), key= lambda x:x[1], reverse=False)
    query_data_timestamps_order = sorted(query_data_timestamps.items(), key= lambda x:x[1], reverse=False)

    print(len(base_data_timestamps_order), len(query_data_timestamps_order))

    base_data_order = np.zeros((len(base_data_timestamps_order), 110, 2))
    query_data_order = np.zeros((len(query_data_timestamps_order), 110, 2))

    i = 0
    for base_order_tuple in base_data_timestamps_order:
        key = base_order_tuple[0]
        base_data_order[i] = base_data[key]
        i+=1

    base_data = []

    i = 0
    start = 0
    query_vector_range = [0]*len(query_data_timestamps_order)
    
    for query_order_tuple in query_data_timestamps_order:
        key = query_order_tuple[0]
        q_ts = query_order_tuple[1]
        query_data_order[i] = query_data[key]
        
        base_v_count = 0
        for j in range(start, len(base_data_timestamps_order)):
            b_ts = base_data_timestamps_order[j][1]
            if b_ts <= q_ts:
                base_v_count += 1

                if j == len(base_data_timestamps_order) - 1:
                    query_vector_range[i] = start + base_v_count - 1
                    break
            else:
                query_vector_range[i] = start + base_v_count - 1
                start = j
                break


        i+=1

    query_data=[]
    print(query_vector_range, len(query_vector_range))

    

    normal_dist = dist.Normal(0, 1)
    base_embeddings = np.zeros((len(base_data_order), vector_embedding_dim),dtype=np.float32)
    i = 0
    for base_v in base_data_order:
        embedding = Transformer_Embedding.get_single_embedding(base_v)
        
        noise_tensor = normal_dist.sample((1,256))
        embedding = embedding + noise_tensor

        embedding_ = embedding.detach().numpy()
        base_embeddings[i] = embedding_
        i += 1
        print(i)
    print('base finished')

    query_embeddings = np.zeros((len(query_data_order), vector_embedding_dim),dtype=np.float32)
    i = 0
    for query_v in query_data_order:
        embedding = Transformer_Embedding.get_single_embedding(query_v)
        noise_tensor = normal_dist.sample((1,256))
        embedding = embedding + noise_tensor

        embedding_ = embedding.detach().numpy()
        query_embeddings[i] = embedding_
        i += 1
        print(i)
    print('query finished')

    base_embedding_file_path = 'embeddings/test/base_embeddings.bin'
    query_embedding_file_path = 'embeddings/test/query_embeddings.bin'
    write_bin_file(base_embedding_file_path, base_embeddings, len(base_embeddings))
    write_bin_file(query_embedding_file_path, query_embeddings, len(query_embeddings))

    with open('embeddings/test/query_vector_range.bin', 'wb') as file:
        file.write(len(query_vector_range).to_bytes(4, byteorder='little', signed= False))

        for qv_range in query_vector_range:
            file.write(struct.pack('i', qv_range))

    genereteTruthSplittedByRide(base_embeddings, query_embeddings, query_vector_range, ride, K)
        

                    

                    
def read_csv_location_data(file_path):
    global data_sum,query_data_sum 

    data = pd.read_csv(file_path)
    groups = data.groupby(data['track_id'])


    for name,group in groups:
        print(name)

        if name == 'AV':
            group_index = group.index.to_list()
            #print(group_index)

            if len(group_index) < min_vector_len:
                continue

            last_element = group_index[int(group.size / group.columns.size) -1]
            #print(last_element)

            trajectory_timestamp = group.start_timestamp[last_element]+100000000.0*group.timestep[last_element]
            print(trajectory_timestamp)
            ref_point_x, ref_point_y = group.position_x[last_element], group.position_y[last_element]

            min_timestep = 109
            max_timestep = 0

            for g_index in group_index:
                timestep = group.timestep[g_index]
                min_timestep = min(min_timestep,timestep)
                max_timestep = max(max_timestep,timestep)

                position_x = group.position_x[g_index]
                position_y = group.position_y[g_index]
            
                query_data[query_data_sum,int(timestep),0] = position_x - ref_point_x
                query_data[query_data_sum,int(timestep),1] = position_y - ref_point_y

            query_data_timestamps[query_data_sum] = trajectory_timestamp
            
            query_data[query_data_sum, 0:int(min_timestep), :] = 0.0
            query_data[query_data_sum, int(max_timestep+1): , :] = 0.0

            
            query_data_sum += 1
            print(query_data_sum)
        else:
            group_index = group.index.to_list()
            #print(group_index)

            if len(group_index) < min_vector_len:
                continue

            last_element = group_index[int(group.size / group.columns.size) -1]
            #print(last_element)


            trajectory_timestamp = group.start_timestamp[last_element]+100000000.0*group.timestep[last_element]
            print(trajectory_timestamp)
            ref_point_x, ref_point_y = group.position_x[last_element], group.position_y[last_element]
           
            min_timestep = 109
            max_timestep = 0

            for g_index in group_index:

                timestep = group.timestep[g_index]
                min_timestep = min(min_timestep,timestep)
                max_timestep = max(max_timestep,timestep)

                position_x = group.position_x[g_index]
                position_y = group.position_y[g_index]
                

                base_data[data_sum,int(timestep),0] = position_x - ref_point_x
                base_data[data_sum,int(timestep),1] = position_y - ref_point_y
            
            base_data_timestamps[data_sum] = trajectory_timestamp
            
            base_data[data_sum, 0:int(min_timestep), :] = 0.0
            base_data[data_sum, int(max_timestep+1): , :] = 0.0

            
            data_sum += 1
            print(data_sum)

    mem = psutil.virtual_memory()
    print('memory:')
    print(mem.percent)



if __name__ == '__main__':
    base_file_path = 'D:/datasets/argoverse2/motion forecast/test/'
    read_csv(base_file_path)

import numpy as np
import struct
import random
import math
import h5py
import os

folder_path = "/home/ubis/datasets/cohere1m/"


def read_hdf5_file(file_path):
    with h5py.File(file_path, 'r') as f:
        base_vectors = np.array(f['train'])
        query_vectors = np.array(f['test'])
        print(f"Loaded: {len(base_vectors)} vectors, {len(query_vectors)} queries")
        return base_vectors, query_vectors

def write_bin_file(file_path, data_array, num, dim):
    with open(file_path, 'wb') as file:
        
        file.write(num.to_bytes(4, byteorder='little', signed= False))
        file.write(dim.to_bytes(4, byteorder='little', signed= False))
    
        for i in range(num):

            for j in range(len(data_array[i])):
                file.write(struct.pack('f', data_array[i][j]))

def L2dis(base_embedding, query_embedding):
    if(len(base_embedding) != len(query_embedding)):
        return -1.0
    
    base_arr = np.array(base_embedding)
    query_arr = np.array(query_embedding)
    
    distance = np.linalg.norm(base_arr - query_arr)
    return distance

def genereteTruthSplittedByRide(base_embeddings, query_embeddings, query_vector_range, ride, K):
    if not os.path.exists(folder_path + "truths"):
        os.makedirs(folder_path + "truths")

    i = 0
    old_i = 0
    truthPath = folder_path + "truths/truth_embeddings_"

    split = -1
    #query_truth_top_k = np.zeros((len(query_embeddings), K))
    query_truth_top_k_num = np.zeros((len(query_embeddings)), dtype=int)
    query_truth_top_k_dis = np.zeros((len(query_embeddings), K))
    query_truth_top_k_index = np.zeros((len(query_embeddings), K), dtype=int)
    query_truth_top_k_max = np.zeros((len(query_embeddings)))

    while i < len(query_embeddings):
        
        old_split = split
        split = int(query_vector_range[i])
        
        sub_base_embeddings = base_embeddings[0:split+1]#[0, split] are valid

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






if __name__ == '__main__':

    base_vecs, query_vecs = read_hdf5_file(folder_path + "glove-200-angular.hdf5")

    write_bin_file(folder_path+"base_embeddings.bin", base_vecs, base_vecs.shape[0], base_vecs.shape[1])
    write_bin_file(folder_path+"query_embeddings.bin", query_vecs, query_vecs.shape[0], query_vecs.shape[1])

    query_vector_range = [0]*len(query_vecs)

    center = base_vecs.shape[0] / query_vecs.shape[0]
    sigma = 10  

    for i in range(len(query_vecs)):
        random_integer = int(random.gauss(center*(i+1), sigma))
        if i < len(query_vecs) - 1:           
            query_vector_range[i] = random_integer - 1
        else:
            query_vector_range[i] = base_vecs.shape[0] - 1
    
    query_vector_range_bin_file = folder_path + "query_vector_range.bin"
    with open(query_vector_range_bin_file, 'wb') as file:
        file.write(len(query_vector_range).to_bytes(4, byteorder='little', signed= False))

        for qv_range in query_vector_range:
            file.write(struct.pack('i', qv_range))

    ride = 100
    K = 10
    genereteTruthSplittedByRide(base_vecs, query_vecs, query_vector_range, ride, K)

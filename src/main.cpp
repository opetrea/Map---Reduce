#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <pthread.h>
#include <memory>
#include <set>
#include <queue>
#include <cstring>
#include <map>
#include <algorithm>

using namespace std;

struct MappData {
    queue<string> files_to_process;
    unsigned int nr_files_to_process;
    unsigned int nr_files_processed;
    pthread_mutex_t queue_mutex;
    pthread_mutex_t result_mutex;
    pthread_barrier_t barrier;
    vector<pair<string, int>> **result;
    unsigned int nr_mappers;
    unsigned int nr_words;
};

struct MapperArgs {
    MappData *data;
    int thread_id;
};

struct ReducerData {
    MappData *data;
    int thread_id;
    unsigned int nr_reducers;
};

// Functie care comverteste toate literele mari la litere mici
string to_lower_case(string word) {
    for (char &c : word) {
        if (c >= 'A' && c <= 'Z') {
            c = c + 32;
        }
    }
    return word;
}

// Functie care elimina caracterele care nu sunt litere
string only_alphabetical(string word) {
    string alphabet = "abcdefghijklmnopqrstuvwxyz";
    string result;
    for (char c : word) {
        if (strchr(alphabet.c_str(), c) != NULL) {
            result += c;
        }
    } 
    return result;
}

// Functie care implementeaza functionalitatea mapper
void *mapper(void *arg) {
    MapperArgs *args = (MapperArgs *)arg;
    MappData *data = args->data;
    int thread_id = args->thread_id;

    while (1) {
        // Pun lock pe mutex pentru a nu accesa 2 sau mai multe thread-uri
        // coada in acelasi timp
        pthread_mutex_lock(&data->queue_mutex);
        string current_file;


        if (data->files_to_process.empty()) {
            pthread_mutex_unlock(&data->queue_mutex);
            break;
        }

        current_file = data->files_to_process.front();
        data->files_to_process.pop();
        data->nr_files_processed++;
        data->nr_files_to_process--;
        int file_d = data->nr_files_processed;
        pthread_mutex_unlock(&data->queue_mutex);

        current_file = "../checker/" + current_file;
        ifstream file(current_file);
        if (!file.is_open()) {
            cerr << "Error: Could not open file??? " << current_file << endl;
            continue;
        }

        string word;
        // Folosesc un set pentru a retine cuvintele unice
        set<string> unique_words;
        while(file >> word) {
            word = to_lower_case(word);
            word = only_alphabetical(word);

            // Daca cuvantul a fost gol sau format din caractere invalide, ignor
            if (word.empty()) {
                continue;
            }

            unique_words.insert(word);
        }

        // Pun lock pe mutex pentru a nu accesa 2 sau mai multe thread-uri
        pthread_mutex_lock(&data->result_mutex);

        for (const auto& word : unique_words) {
            data->result[thread_id]->push_back({word, file_d});
        }
        data->nr_words += unique_words.size();
        pthread_mutex_unlock(&data->result_mutex);

        file.close();
    }

    // Folosesc o bariera; toate thread-urile mapper o sa-si termineexecutia
    // in acelasi timp
    pthread_barrier_wait(&data->barrier);

    return nullptr;
}

void *reducer(void *arg) {
    ReducerData *data = (ReducerData *)arg;
    MappData *mapp_data = data->data;

    // Pun barieira; abia cand numarul total de thread-uri vor ajunge la bariera
    // thread-urile reducer vor vorni cu adevarat executia
    pthread_barrier_wait(&mapp_data->barrier);

    // Calculez numarul de litere care ii vor fi distribuite fiecarui thread
    // reducer si indecsii de start si de final
    int base_letters = 26 / data->nr_reducers;
    int extra_letters = 26 % data->nr_reducers;
    
    int start_letter, letters_to_process;
    
    if (data->thread_id < extra_letters) {
        letters_to_process = base_letters + 1;
        start_letter = data->thread_id * letters_to_process;
    } else {
        letters_to_process = base_letters;
        start_letter = data->thread_id * letters_to_process + extra_letters;
    }
    int end_letter = start_letter + letters_to_process - 1;

    // Creez un map, unde la cheie1: litera, am un o valoare de tip map,
    // cheie2: cuvant si valoare int, indexul fisierului
    map<char, map<string, set<int>>> letter_words;

    pthread_mutex_lock(&mapp_data->result_mutex);
    // Parcurg rezultatele thread-urilor mapper si le pun in map

    for (unsigned int i = 0; i < mapp_data->nr_mappers; i++) {
        for (const auto& pair : *mapp_data->result[i]) {
            char c = pair.first[0];
            if (c - 'a' >= start_letter && c - 'a' <= end_letter) {
                letter_words[c][pair.first].insert(pair.second);
            }
        }
    }

    pthread_mutex_unlock(&mapp_data->result_mutex);

    // Fiecare thread reducer se va ocupa doar de anumite litere
    for (int i = start_letter; i <= end_letter; i++) {
        char c = 'a' + i;
        // Creez fisierul de output pt litera curenta
        string filename_output = string(1, c) + string(".txt");

        // Ma folosesc de un vector de perechi pentru a sorta rezultatele din map
        vector<pair<string, set<int>>> words_vector;
        for (const auto& pair : letter_words[c]) {
            words_vector.push_back({pair.first, pair.second});
        }

        // Sortez vectorul in functie de numarul de aparitii ale cuvintelor, iar
        // in caz de egalitate, lexicografic
        sort(words_vector.begin(), words_vector.end(), [](
            const pair<string,set<int>>& a,
            const pair<string,set<int>>& b) {
            if (a.second.size() != b.second.size()) {
                return a.second.size() > b.second.size();
            }
            return a.first < b.first;
        });

        ofstream output_file(filename_output, ios::app);
        if (!output_file.is_open()) {
            cerr << "Error: Could not open file " << filename_output << endl;
            continue;
        }

        // Fac scrierea in fisiere
        for (const auto& pair : words_vector) {
            output_file << pair.first << ":[";

            bool need_space = false;

            for (int file : pair.second) {
                if (need_space == false) {
                    output_file << file;
                    need_space = true;
                } else {
                output_file << " " << file;
                }
            }
            output_file << "]" << endl;
        }

        output_file.close();
    }

    return nullptr;
}

void create_threads(vector<pthread_t> &threads, unsigned int nr_mappers, unsigned int nr_reducers, MappData *mapp_data)
{
    vector<MapperArgs> mapper_args(nr_mappers);
    vector<ReducerData> reducer_args(nr_reducers);

    unsigned int total_threads = nr_mappers + nr_reducers;

    // Initializez o baiera pentru a forta thread-urile reducer sa astepte
    // terminarea executiei thread-urilor mapper
    pthread_barrier_init(&mapp_data->barrier, nullptr, nr_mappers + nr_reducers);
    for (unsigned int i = 0; i < total_threads; i++) {
        if (i < nr_mappers) {
            mapper_args[i].data = mapp_data;
            mapper_args[i].thread_id = i;
            pthread_create(&threads[i], nullptr, mapper, &mapper_args[i]);
        } else {
            reducer_args[i - nr_mappers].data = mapp_data;
            reducer_args[i - nr_mappers].thread_id = i - nr_mappers;
            reducer_args[i - nr_mappers].nr_reducers = nr_reducers;
            pthread_create(&threads[i], nullptr, reducer, &reducer_args[i - nr_mappers]);
        }
    }

    for (auto &thread : threads) {
        pthread_join(thread, nullptr);
    }

}

int main(int argc, char **argv) {
    unsigned int nr_mappers;
    unsigned int nr_reducers;
    string input_file;

    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <nr_mappers> <nr_reducers> <input_file>" << endl;
        return 1;
    }

    // Parseaza argumentele
    nr_mappers = stoi(argv[1]);
    nr_reducers = stoi(argv[2]);
    input_file = argv[3];

    unsigned int total_nr_threads = nr_mappers + nr_reducers;

    // Deschid fisierul de input
    ifstream file(input_file);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << input_file << endl;
        return 1;
    }

    unsigned int nr_files_to_process;
    file >> nr_files_to_process;

    // Creez o variabila de tip mappdata care sa reprezinte contextul comun
    // Al thread-urilor mapper
    struct MappData mapp_data;
    mapp_data.nr_files_to_process = nr_files_to_process;
    mapp_data.nr_files_processed = 0;
    mapp_data.nr_mappers = nr_mappers;
    mapp_data.nr_words = 0;
    // Aloc memorie pentru vectorul de rezultate
    mapp_data.result = new vector<pair<string, int>>*[nr_mappers];
    for (unsigned int i = 0; i < nr_mappers; i++) {
        mapp_data.result[i] = new vector<pair<string, int>>();
    }

    // Creez 2 mutex-uri, unul pentru coada de fisiere si unul pentru rezultate
    pthread_mutex_init(&mapp_data.queue_mutex, nullptr);
    pthread_mutex_init(&mapp_data.result_mutex, nullptr);

    for (unsigned int i = 0; i < nr_files_to_process; ++i) {
        string file_name;
        file >> file_name;
        // Citesc fisierele care trebuie procesate si le pun intr-o coada
        mapp_data.files_to_process.push(file_name);
    }

    vector<pthread_t> threads(total_nr_threads);
    create_threads(threads, nr_mappers, nr_reducers, &mapp_data);
    pthread_mutex_destroy(&mapp_data.queue_mutex);
    pthread_mutex_destroy(&mapp_data.result_mutex);

    for (unsigned int i = 0; i < nr_mappers; i++) {
        delete mapp_data.result[i];
    }

    delete[] mapp_data.result;
    file.close();
    return 0;
}
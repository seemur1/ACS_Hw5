#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include <cstring>   // для функций strcmp
#include <ctime>     // для функций time()
#include <sys/stat.h> // Для функций stat

// Названия папок, в которые будут записываться результаты работы программ
const char test[100] = "./../../Tests/", res[100] = "./../../Results/";

FILE *fp;
pthread_rwlock_t rwlock ;  //блокировка чтения-записи

bool** garden_map;
// Хотел использовать несколько массивов булей, но они занимают столько же памяти, сколько и int => решил ограничиться char
// (если бы было можно использовать vector<bool> - использовал бы его.)
char** visited;

// Метод, "ругающийся" при некорректном кол-ве параметров на вход.
void errMessage1() {
    printf("incorrect command line!\n  Waited:\n     command -f size_x size_y f_speed s_speed infile outfile\n");
    printf("  Or:\n     command -n size_x size_y f_speed s_speed probability outfile\n");
}

// Метод, "ругающийся" при некорректном типе читаемых значений.
void errMessage2() {
    printf("incorrect qualifier value!\n  Waited:\n     command -f size_x size_y f_speed s_speed infile outfile\n");
    printf("  Or:\n     command -n size_x size_y f_speed s_speed probability outfile\n");
}

// Метод, проверяющий режим ввода карты сада + инициализирующий его размеры.
bool isFile(char* argv[], int * size_x, int * size_y, int * f_speed, int * s_speed, int * probability) {
    *size_x = atoi(argv[2]);
    *size_y = atoi(argv[3]);
    *f_speed = atoi(argv[4]);
    *s_speed = atoi(argv[5]);
    *probability = atoi(argv[6]);
    return !strcmp(argv[1], "-f");
}

// Метод, инициализирующий все массивы, описывающие св-ва сада.
void Init(int size_x, int size_y) {
    // Иницициализируем все поля сада.
    garden_map = new bool*[size_x], visited = new char*[size_x];
    for (int i = 0; i < size_x; ++i) {
        garden_map[i] = new bool[size_y], visited[i] = new char[size_y];
        // Заполняем пустыми значениями массив "посещённых" клеток.
        for (int j = 0; j < size_y; ++j) {
            visited[i][j] = '0';
        }
    }
}

// Метод, заполняющий garden_map соотв. значениями из файла.
bool In(FILE* fp, int size_x, int size_y) {
    for (int i = 0; i < size_x * size_y; ++i) {
        int status;
        // Если встретили конец файла раньше ожидаемого / встретили не "тот" символ - ругаемся, выходим.
        if ((fscanf(fp, "%d", &status) == EOF) || (status < 0) || (status > 1)){ return false; }
        garden_map[i % size_x][i / size_x] = status;
    }
    // Если ни разу не "наругались" - возвращаем true.
    return true;
}

// Метод, рандомно создающий карту сада.
void InRnd(int size_x, int size_y, int probability) {
    for (int i = 0; i < size_x * size_y; ++i) {
        garden_map[i % size_x][i / size_x] = ((rand() % 10) < probability);
    }
}

//стартовая функция потоков-садовников.
void *gardenerJob(void *param) {
    bool mode = true, is_visited, need_to_process;
    // Инициализирууем все необходимые значения.
    int id = *((int*)param), size_x = *((int*)param + 1), size_y = *((int*)param + 2),
            speed = *((int*)param + 3), is_vertical = *((int*)param + 4);
    // Инициализирууем стартовые позиции.
    int pos_x = (is_vertical) ? size_x - 1 : 0, pos_y = (is_vertical) ? size_y - 1 : 0, count_x = 0, count_y = 0, count = 0;

    // Обходим всю карту.
    while (count < size_x * size_y) {
        //закрыть блокировку для чтения
        pthread_rwlock_rdlock(&rwlock) ;
        //прочитать данные из общего массива – критическая секция
        is_visited = (visited[pos_x][pos_y] != '0');
        need_to_process = garden_map[pos_x][pos_y];
        // Обновляем статус клетки.
        visited[pos_x][pos_y] += (is_visited) ? 0 : id;
        // обращаемся к файлу вывода, и выводим туда результаты работы нашего садовника.
        if (is_visited) {
            fprintf(fp, "time = %lu ---> Садовник %d: Клетка[%d][%d] обработана садовником %d!!!\n",
                    clock(), id, pos_x, pos_y, (id == 1) ? 2 : 1);
        } else {
            fprintf(fp, "time = %lu ---> Садовник %d: Клетка[%d][%d] %s!!!\n",
                    clock(), id, pos_x, pos_y, (need_to_process) ? "была нами обработана!" : "пройдена мимо!");
        }
        //открыть блокировку
        pthread_rwlock_unlock(&rwlock) ;
        if (is_visited) {
            printf("time = %lu ---> Садовник %d: Клетка[%d][%d] обработана садовником %d!!!\n",
                   clock(), id, pos_x, pos_y, (id == 1) ? 2 : 1);
        } else {
            printf("time = %lu ---> Садовник %d: Клетка[%d][%d] %s!!!\n",
                   clock(), id, pos_x, pos_y, (need_to_process) ? "была нами обработана!" : "пройдена мимо!");
        }
        // Ходьба (изменение координат):
        pos_x = (is_vertical) ? pos_x - (count_y + 1) / size_y :
                ((size_x + pos_x + ((mode) ? 1 : -1) + ((count_x + 1) / size_x) * ((!mode) ? 1 : -1)) % size_x);
        pos_y = (!is_vertical) ? pos_y + (count_x + 1) / size_x :
                ((size_y + pos_y + ((mode) ? -1 : 1) + ((count_y + 1) / size_y) * ((!mode) ? -1 : 1)) % size_y);
        // Меняем режим работы, при достижении "границ".
        mode = (is_vertical) ? (((count_y + 1) / size_y) ? !mode : mode)
                             : (((count_x + 1) / size_x) ? !mode : mode);
        count_x = (count_x + 1) % size_x;
        count_y = (count_y + 1) % size_y;
        ++count;
        sleep(speed);
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    if(argc != 8) {
        errMessage1();
        return 1;
    }
    printf("Start\n");

    int size_x, size_y, f_speed, s_speed, probability;

    // Собираем адреса файлов.
    char testDir[200], resDir[200];
    strcpy(testDir, test);
    strcpy(resDir, res);
    // Задаём Директории хранения (для тестов и результатов).
    strcat(testDir, argv[6]);
    strcat(resDir, argv[7]);

    // Проверка иcточника данных
    if(isFile(argv, &size_x, &size_y, &f_speed, &s_speed, &probability)) {
        struct stat buffer{};
        if (stat(testDir, &buffer)) {
            printf("incorrect Name Of File!\n");
            return 4;
        }
        fp = fopen(testDir, "r");
        // Проверяем размеры сада.
        if((size_x < 1) || (size_x > 10000)) {
            printf("incorrect size of garden = %d. Set 0 < size_x <= 10000\n", size_x);
            return 3;
        }
        if((size_y < 1) || (size_y > 10000)) {
            printf("incorrect size of garden = %d. Set 0 < size_y <= 10000\n", size_y);
            return 3;
        }
        // Инициализируем массивы требуемых рамеров.
        Init(size_x, size_y);
        // Считываем план сада.
        if (!In(fp, size_x, size_y)) {
            printf("incorrect input. Set 0 <= garden_map[x][y] <= 1\n1 - block is being gardened, 0 - is not.");
            return 5;
        }
        fclose(fp);
    } else {
        if (!strcmp(argv[1], "-f") && !strcmp(argv[1], "-n")) {
            errMessage2();
            return 2;
        }
        // Инициализируем массивы требуемых рамеров.
        Init(size_x, size_y);
        // cиcтемные чаcы в качеcтве инициализатора
        srand((unsigned int)(time(0)));
        // Заполнение контейнера генератором cлучайных чиcел
        InRnd(size_x, size_y, probability);
    }

    printf("size_x = %d, size_y = %d\n", size_x, size_y);
    // Вывод cодержимого контейнера в файл
    fp = fopen(resDir, "w+");
    // Начало вывода работы садовников.
    fprintf(fp, "Gardeners Started!:\n");

    //инициализация блокировки чтения-записи
    pthread_rwlock_init(&rwlock, nullptr) ;

    //создание двух садовников
    pthread_t threadG[2] ;
    // инициализируем массив их параметров.
    // 1 - id, 2 - size_x, 3 - size_y, 4 - speed, 5 - is_vertical (0 - нет; 1 - да).
    int gardener_params[10] = {1, size_x, size_y, f_speed, 0, 2, size_x, size_y, s_speed, 1};
    // Создаём потоки, обрабатывающие работу двух садовников.
    for (int i = 0 ; i < 2 ; i++) {
        pthread_create(&threadG[i], nullptr, gardenerJob, (void*)(gardener_params + i * 5)) ;
    }
    // Ожидаем завершения работы двух садовников.
    pthread_join(threadG[0], nullptr);
    pthread_join(threadG[1], nullptr);

    printf("finish");

    fclose(fp);

    return 0;
}

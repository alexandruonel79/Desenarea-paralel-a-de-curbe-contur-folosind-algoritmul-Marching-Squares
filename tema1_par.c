// Author: APD team, except where source was noted
#define _POSIX_C_SOURCE 200112L /* Or higher */

#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define CONTOUR_CONFIG_COUNT 16
#define FILENAME_MAX_SIZE 50
#define STEP 8
#define SIGMA 200
#define RESCALE_X 2048
#define RESCALE_Y 2048

#define CLAMP(v, min, max) \
    if (v < min)           \
    {                      \
        v = min;           \
    }                      \
    else if (v > max)      \
    {                      \
        v = max;           \
    }
// holds all the necessary information for each thread
typedef struct ThreadArgs
{
    long thread_id;
    int thread_count;
    ppm_image *image;
    ppm_image *new_image;
    ppm_image *scaled_image;
    unsigned char **grid;
    ppm_image **contour_map;
    pthread_barrier_t *barrier;
} ThreadArgs;

// a simple min function :)
int min(int x, int y)
{
    if (x >= y)
        return y;
    else
        return x;
}

// alocates memory for a new grid
unsigned char **allocMemoryForNewGrid(ppm_image *image)
{
    int p = image->x / STEP;
    int q = image->y / STEP;

    unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char *));
    if (!grid)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i <= p; i++)
    {
        grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i])
        {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }

    return grid;
}

// alloc memory for image
ppm_image *allocMemoryForNewImage()
{
    ppm_image *new_image = (ppm_image *)malloc(sizeof(ppm_image));
    if (!new_image)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    new_image->x = RESCALE_X;
    new_image->y = RESCALE_Y;

    new_image->data = (ppm_pixel *)malloc(new_image->x * new_image->y * sizeof(ppm_pixel));
    if (!new_image)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    return new_image;
}

// Creates a map between the binary configuration (e.g. 0110_2) and the corresponding pixels
// that need to be set on the output image. An array is used for this map since the keys are
// binary numbers in 0-15. Contour images are located in the './contours' directory.
ppm_image **init_contour_map()
{
    ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!map)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++)
    {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        map[i] = read_ppm(filename);
    }

    return map;
}

// Calls `free` method on the utilized resources.
void free_resources(ppm_image *image, ppm_image **contour_map, unsigned char **grid, int step_x)
{
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++)
    {
        free(contour_map[i]->data);
        free(contour_map[i]);
    }
    free(contour_map);

    for (int i = 0; i <= image->x / step_x; i++)
    {
        free(grid[i]);
    }
    free(grid);

    free(image->data);
    free(image);
}

// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.
void update_image(ppm_image *image, ppm_image *contour, int x, int y)
{
    for (int i = 0; i < contour->x; i++)
    {
        for (int j = 0; j < contour->y; j++)
        {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
}

// Corresponds to step 1 of the marching squares algorithm, which focuses on sampling the image.
// Builds a p x q grid of points with values which can be either 0 or 1, depending on how the
// pixel values compare to the `sigma` reference value. The points are taken at equal distances
// in the original image, based on the `step_x` and `step_y` arguments.
unsigned char **sample_grid(ppm_image *image, int step_x, int step_y, unsigned char sigma,
                            unsigned char **grid, long ID, int thread_count)
{
    int p = image->x / step_x;
    int q = image->y / step_y;
    // used the formula from the laboratory to succesfully parallel
    int start = ID * (double)p / thread_count;
    int end = min((ID + 1) * (double)p / thread_count, p);

    for (int i = start; i < end; i++)
    {
        for (int j = 0; j < q; j++)
        {
            ppm_pixel curr_pixel = image->data[i * step_x * image->y + j * step_y];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > sigma)
            {
                grid[i][j] = 0;
            }
            else
            {
                grid[i][j] = 1;
            }
        }
    }

    // last sample points have no neighbors below / to the right, so we use pixels on the
    // last row / column of the input image for them
    for (int i = start; i < end; i++)
    {
        ppm_pixel curr_pixel = image->data[i * step_x * image->y + image->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma)
        {
            grid[i][q] = 0;
        }
        else
        {
            grid[i][q] = 1;
        }
    }
    // used the formula from the laboratory to succesfully parallel
    int start_q = ID * (double)q / thread_count;
    int end_q = min((ID + 1) * (double)q / thread_count, q);

    for (int j = start_q; j < end_q; j++)
    {
        ppm_pixel curr_pixel = image->data[(image->x - 1) * image->y + j * step_y];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma)
        {
            grid[p][j] = 0;
        }
        else
        {
            grid[p][j] = 1;
        }
    }

    return grid;
}

// Corresponds to step 2 of the marching squares algorithm, which focuses on identifying the
// type of contour which corresponds to each subgrid. It determines the binary value of each
// sample fragment of the original image and replaces the pixels in the original image with
// the pixels of the corresponding contour image accordingly.
void march(ppm_image *image, unsigned char **grid, ppm_image **contour_map, int step_x, int step_y,
           int thread_count, long ID)
{
    int p = image->x / step_x;
    int q = image->y / step_y;
    // used the formula from the laboratory to succesfully parallel
    int start = ID * (double)p / thread_count;
    int end = min((ID + 1) * (double)p / thread_count, p);

    for (int i = start; i < end; i++)
    {
        for (int j = 0; j < q; j++)
        {
            unsigned char k = 8 * grid[i][j] + 4 * grid[i][j + 1] + 2 * grid[i + 1][j + 1] + 1 * grid[i + 1][j];
            update_image(image, contour_map[k], i * step_x, j * step_y);
        }
    }
}

ppm_image *rescale_image(ppm_image *image, long ID, int thread_count, ppm_image *new_image)
{
    uint8_t sample[3];

    // we only rescale downwards
    if (image->x <= RESCALE_X && image->y <= RESCALE_Y)
    {
        return image;
    }
    // used the formula from the laboratory to succesfully parallel
    int start = ID * (double)new_image->x / thread_count;
    int end = min((ID + 1) * (double)new_image->x / thread_count, new_image->x);

    // use bicubic interpolation for scaling
    for (int i = start; i < end; i++)
    {
        for (int j = 0; j < new_image->y; j++)
        {
            float u = (float)i / (float)(new_image->x - 1);
            float v = (float)j / (float)(new_image->y - 1);
            sample_bicubic(image, u, v, sample);

            new_image->data[i * new_image->y + j].red = sample[0];
            new_image->data[i * new_image->y + j].green = sample[1];
            new_image->data[i * new_image->y + j].blue = sample[2];
        }
    }

    return new_image;
}

// parallel starting point function for threads
// same name as in lab
void *f(void *args)
{
    // get all the thread arguments
    long id = (*(ThreadArgs *)args).thread_id;
    long thread_count = (*(ThreadArgs *)args).thread_count;
    ppm_image *image = (*(ThreadArgs *)args).image;
    ppm_image *new_image = (*(ThreadArgs *)args).new_image;
    unsigned char **grid = (*(ThreadArgs *)args).grid;
    ppm_image **contour_map = (*(ThreadArgs *)args).contour_map;
    pthread_barrier_t *barrier = (*(ThreadArgs *)args).barrier;

    // 1. Rescale the image
    ppm_image *scaled_image = rescale_image(image, id, thread_count, new_image);

    // barrier to await the scaled image
    pthread_barrier_wait(barrier);

    // 2. Sample the grid
    int step_x = STEP;
    int step_y = STEP;

    grid = sample_grid(scaled_image, step_x, step_y, SIGMA, grid, id, thread_count);

    // barrier to await the grid
    pthread_barrier_wait(barrier);

    // 3. March the squares
    march(scaled_image, grid, contour_map, step_x, step_y, thread_count, id);

    // make the pointers accesible from main
    (*(ThreadArgs *)args).scaled_image = scaled_image;
    (*(ThreadArgs *)args).grid = grid;
    (*(ThreadArgs *)args).contour_map = contour_map;

    pthread_exit(NULL);
}
int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }
    // getting the thread count from argv
    int thread_count = atoi(argv[3]);
    // initializing the barrier, will be used in the thread
    // starting function
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, thread_count);

    ppm_image *image = read_ppm(argv[1]);
    int step_x = STEP;

    // 0. Initialize contour map
    ppm_image **contour_map = init_contour_map();
    // allocating memory for the new_image
    ppm_image *new_image = allocMemoryForNewImage();
    // allocating memory for the grid
    unsigned char **grid = NULL;
    grid = allocMemoryForNewGrid(new_image);

    pthread_t threads[thread_count];
    int r;
    long id;
    void *status;
    ThreadArgs arguments[thread_count];

    // creating the threads
    for (id = 0; id < thread_count; id++)
    {
        arguments[id].thread_id = id;
        arguments[id].image = image;
        arguments[id].new_image = new_image;
        arguments[id].thread_count = thread_count;
        arguments[id].scaled_image = NULL;
        arguments[id].grid = grid;
        arguments[id].contour_map = contour_map;
        arguments[id].barrier = &barrier;

        r = pthread_create(&threads[id], NULL, f, (void *)&arguments[id]);

        if (r)
        {
            printf("Eroare la crearea thread-ului %ld\n", id);
            exit(-1);
        }
    }
    // joining all threads
    for (id = 0; id < thread_count; id++)
    {
        r = pthread_join(threads[id], &status);

        if (r)
        {
            printf("Eroare la asteptarea thread-ului %ld\n", id);
            exit(-1);
        }
    }
    // updating with the newly multithreaded computed values
    ppm_image *scaled_image = arguments[0].scaled_image;
    grid = arguments[0].grid;
    contour_map = arguments[0].contour_map;

    // 4. Write output
    write_ppm(scaled_image, argv[2]);

    // freeing the initial image
    if (!(image->x <= RESCALE_X && image->y <= RESCALE_Y))
    {
        free(image->data);
        free(image);
    }
    else
    {
        free(new_image->data);
        free(new_image);
    }

    // free all the resources
    free_resources(scaled_image, contour_map, grid, step_x);

    // destroy the used barrier
    pthread_barrier_destroy(&barrier);

    return 0;
}

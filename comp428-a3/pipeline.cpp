#include "functions.h"
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Matrix of size n x n
#define N 8

#define MIN 1
#define MAX 10

#define MASTER 0

int GetRow(int rank, int m)
{
	return rank % (N/m);
}

int GetColumn(int rank, int m)
{
	return (rank * m) / N;
}

int GetRank(int row, int column, int m)
{
	return row + (N / m) * column;
}

int Sqrt(int n)
{
	if (n == 4)
		return 2;
	else if (n == 9)
		return 3;
	else if (n == 16)
		return 4;
	else if (n == 25)
		return 5;
	else if (n == 36)
		return 6;
	else if (n == 49)
		return 7;
	else if (n == 64)
		return 8;
	else if (n > 64)
	{
		printf("Error! Only supports up to n = 64!\n");
		exit(1);
	}
	else
	{
		printf("Error! Only supports perfect squares!\n");
		exit(1);
	}
}

int main(int argc, char* argv[])
{
	int size, rank;
	int row, column;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	// int matrix[N][N];
	int** matrix = new int*[N];
	for (int i = 0; i < N; ++i)
	{
		matrix[i] = new int[N];
	}

	int* globalArray = new int[N*N];

	srand(time(NULL));

	if (rank == MASTER)
	{
		printf("Processor %d: Creating global matrix.\n", rank);
		RandomMatrix(matrix, N, MIN, MAX);
		InitializeShortestPathMatrix(matrix, N);

		printf("Initial Matrix:\n");
		PrintMatrix(matrix, N);

		MatrixToArray(matrix, N, globalArray);
	}
	
	// printf("Processor %d: Receiving global matrix.\n", rank);
	MPI_Bcast(globalArray, N*N, MPI_INT, MASTER, MPI_COMM_WORLD);
	ArrayToMatrix(matrix, N, globalArray);

	// The m x m chunk of each processor.
	// int m = N / ((int) sqrt((double) size));
	int m = N / Sqrt(size);
	row = GetRow(rank, m);
	column = GetColumn(rank, m);
	// printf("Process %d: (row, column) = (%d, %d)\n", rank, row, column);

	if (rank == MASTER)	
	{
		printf("Processor %d: Creating local buffers.\n", rank);
	}
	int** bufferMatrix = new int*[m];
	for (int i = 0; i < m; ++i)
	{
		bufferMatrix[i] = new int[m];
	}

	int* bufferArray = new int[m*m];

	int* bufferRow = new int[m];
	int* bufferColumn = new int[m];

	int blocksInRow, blocksInColumn;
	blocksInRow = blocksInColumn = N/m;

	for (int k = 0; k < N; ++k)
	{

		if (IsInRow(k, row, m))
		{
			for (int i = 0; i < m; ++i)
			{
				bufferRow[i] = matrix[k][m*column + i];
			}

			if (row > 0)
			{
				MPI_Send(bufferRow, m, MPI_INT, GetRank(row - 1, column, m), 0, MPI_COMM_WORLD);
			}
			if (row < blocksInRow - 1)
			{
				MPI_Send(bufferRow, m, MPI_INT, GetRank(row + 1, column, m), 0, MPI_COMM_WORLD);
			}
		}
		else
		{
			MPI_Recv(bufferRow, m, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			if (IsBeforeRow(k, row, m))
			{
				MPI_Send(bufferRow, m, MPI_INT, GetRank(row + 1, column, m), 0, MPI_COMM_WORLD);
			}
			else if (IsAfterRow(k, row, m))
			{
				MPI_Send(bufferRow, m, MPI_INT, GetRank(row - 1, column, m), 0, MPI_COMM_WORLD);
			}
		}

		if (IsInColumn(k, column, m))
		{
			for (int i = 0; i < m; ++i)
			{
				bufferColumn[i] = matrix[m*row+i][k];
			}

			if (column > 0)
			{
				MPI_Send(bufferColumn, m, MPI_INT, GetRank(row, column - 1, m), 0, MPI_COMM_WORLD);
			}
			if (column < blocksInColumn - 1)
			{
				MPI_Send(bufferColumn, m, MPI_INT, GetRank(row, column + 1, m), 0, MPI_COMM_WORLD);
			}
		}
		else
		{
			MPI_Recv(bufferColumn, m, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			if (IsBeforeColumn(k, column, m))
			{
				MPI_Send(bufferColumn, m, MPI_INT, GetRank(row, column + 1, m), 0, MPI_COMM_WORLD);
			}
			else if (IsAfterColumn(k, column, m))
			{
				MPI_Send(bufferColumn, m, MPI_INT, GetRank(row, column - 1, m), 0, MPI_COMM_WORLD);
			}
		}

		if (rank == MASTER)
		{
			printf("Processor %d: Doing local algorithm.\n", rank);
		}

		// Doing the local Floyd algorithm.
		for (int i = 0; i < m; ++i)
		{
			for (int j = 0; j < m; ++j)
			{
				if(matrix[m*row + i][m*column + j] > (bufferColumn[i] + bufferRow[j]))
				{
					matrix[m*row + i][m*column + j] = bufferColumn[i] + bufferRow[j];
				}
			}
		}
	}

	if (rank == MASTER)
	{
		printf("Processor %d: Aggregating matrix.\n", rank);
	}

	for (int i = 1; i < size; ++i)
	{
		if (rank == i)
		{
			FillLocalMatrix(matrix, bufferMatrix, m, row, column);
			MatrixToArray(bufferMatrix, m, bufferArray);
			MPI_Send(bufferArray, m*m, MPI_INT, MASTER, 0, MPI_COMM_WORLD);
		}

		if (rank == MASTER)
		{
			MPI_Recv(bufferArray, m*m, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			ArrayToMatrix(bufferMatrix, m, bufferArray);
			UpdateGlobalMatrix(matrix, bufferMatrix, m, GetRow(i, m), GetColumn(i, m));
		}
	}

	if (rank == MASTER)
	{
		printf("Final Matrix:\n");
		PrintMatrix(matrix, N);
	}

	// Deallocate bufferMatrix and bufferRow and bufferColumn.
	MPI_Finalize();
}

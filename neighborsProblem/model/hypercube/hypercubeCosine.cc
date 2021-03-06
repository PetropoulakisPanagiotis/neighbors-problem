#include <iostream>
#include <vector>
#include <list>
#include <algorithm>
#include <new>
#include <cmath>
#include "hypercube.h"
#include "../../hashFunction/hashFunction.h"
#include "../../item/item.h"
#include "../../utils/utils.h"

using namespace std;

//////////////////////////////////////////////
/* Implementation of hypercube cosine class */
//////////////////////////////////////////////

/* Default constructor */
hypercubeCosine::hypercubeCosine():tableSize(0),n(0),k(5),dim(0),m(5),probes(1),fitted(0){}

hypercubeCosine::hypercubeCosine(int k, int m, int probes, errorCode& status):tableSize(0),n(0),k(k),dim(0),m(m),probes(probes),fitted(0){
    /* Check parameters */
    if(k < MIN_K || k > MAX_K || m < MIN_M || m > MAX_M){
        status = INVALID_PARAMETERS;
        this->k = -1;
    }

    /* Ckeck probes */
    if(probes < MIN_PROBES || probes > pow(2, k)){
        status = INVALID_PARAMETERS;
        this->k = -1;
    }
}

hypercubeCosine::~hypercubeCosine(){ 
    /* Check method */
    if(this->k == -1){
        return;
    }   
    
    /* Delete hash function */
    if(this->fitted == 1)
        delete this->hashFunctions;
}

/* Fix hash function, members of hypercube cosine and add given points in the cube */
void hypercubeCosine::fit(list<Item>& points, errorCode& status){
    int i;
    int pos; // Pos in cube

    /* Iteratiors */
    list<Item>::iterator iterPoints = points.begin(); // Iterate through points
   
    status = SUCCESS;

    /* Check method */
    if(this->k == -1){
        status = INVALID_METHOD;
        return;
    }

    /* Already fitted */
    if(this->fitted == 1){
        status = METHOD_ALREADY_USED;
        return;
    }

    /* Set members */
    this->n = points.size();
    if(this->n < MIN_POINTS || this->n > MAX_POINTS){
        status = INVALID_POINTS;
        return;
    }
    
    /* Set table size */
    this->tableSize = pow(2, this->k);
    this->cube.reserve(this->tableSize);
    
    /* Fix table */
    for(i = 0; i < this->tableSize; i++)
        this->cube.push_back(list<Item>());

    /* Set dimension */
    this->dim = iterPoints->getDim();
    if(this->dim <= 0){
        status = INVALID_DIM;
        return;
    }
    
    ///////////////////////
    /* Set hash function */
    ///////////////////////

    hashFunctionCosine* newFunc = NULL;  

    newFunc = new hashFunctionCosine(this->dim, this->k); 
    if(newFunc == NULL){
        status = ALLOCATION_FAILED;
        this->k = -1;
    }
    
    this->hashFunctions = newFunc; // Add hash function

    //////////////
    /* Set cube */
    //////////////
    
    /* Scan given points */
    for(iterPoints = points.begin(); iterPoints != points.end(); iterPoints++){
        /* Check consistency of dim */
        if(this->dim != iterPoints->getDim()){
            status = INVALID_DIM;
            break;
        }
        
        /* Find position in cube */
        pos = this->hashFunctions->hash(*iterPoints, status);
        if(pos < 0 || pos >= tableSize){
            status = INVALID_HASH_FUNCTION;
            break;
        }

        if(status != SUCCESS){
            this->k = -1;
            break;
        }

        /* Add point */
        this->cube[pos].push_back(*iterPoints);

    } // End for - Points

    /* Error occured - Clear structures */
    if(status != SUCCESS){
       
        /* Clear points */
        for(i = 0; i < this->tableSize; i++)
            this->cube[i].clear();
 
        /* Clear hash function */
        delete this->hashFunctions;      
    }
    else
        /* Method fitted */
        this->fitted = 1;
}

/* Find the radius neighbors of a given point */
void hypercubeCosine::radiusNeighbors(Item& query, int radius, list<Item>& neighbors, list<double>* neighborsDistances, errorCode& status){
    int i, initialPos, pos;
    double currDist; // Distance of a point in list
    list<Item>::iterator iter;
    vector<neighborVertice> neighborVertices; // Keep all neighbors  
    int numNeighbors = 0; // Number of neighbors

    status = SUCCESS;
    
    /* Check parameters */
    if(radius < MIN_RADIUS || radius > MAX_RADIUS){
        status = INVALID_RADIUS;
        return; 
    }

    /* Check model */
    if(this->fitted == 0){
        status = METHOD_UNFITTED;
        return;
    }

    if(this->k == -1){
        status = INVALID_METHOD;
        return;
    }

    /* Clear given lists */
    neighbors.clear();
    if(neighborsDistances != NULL)
        neighborsDistances->clear();

    /* Find initial vertice */
    initialPos = this->hashFunctions->hash(query, status);
    if(status != SUCCESS)
            return;

    /* Find all neighbors of current vertice */
    for(i = 0; i < this->tableSize; i++){

        /* Don't check the same position */
        if(i == initialPos)
            continue;

        neighborVertices.push_back(neighborVertice(hammingDistance(initialPos, i), i));
    } // End for

    /* Sort neighborVertices */
    make_heap(neighborVertices.begin(), neighborVertices.end(), verticesCompare());

    /* Check probes vertices for neighbors */
    for(i = 0; i < this->probes; i++){
   
        /* Check initial pos */
        if(i == 0)
            pos = initialPos;
        /* Extract min pos */
        else
            pos = neighborVertices.front().pos;

        /* Empty vertice */
        if(this->cube[pos].size() == 0)
            continue;

        /* Scan current vertice */
        for(iter = this->cube[pos].begin(); iter != this->cube[pos].end(); iter++){  

            numNeighbors += 1;
            
            /* Find current distance */
            currDist = iter->cosineDist(query, status);
            if(status != SUCCESS)
                return;
            
            /* Keep neighbor */
            if(currDist < radius){
                neighbors.push_back(*iter);
                if(neighborsDistances != NULL)
                    neighborsDistances->push_back(currDist);
            }

            /* Found m neighbors */
            if(numNeighbors == m)
                break;
        } // End for - Scan list

        /* Found m neighbors */
        if(numNeighbors == m)
            break;
    } // End for - Probes
}

/* Find the nearest neighbor of a given point */
void hypercubeCosine::nNeighbor(Item& query, Item& nNeighbor, double* neighborDistance, errorCode& status){
    int i, initialPos, pos, found = 0, flag = 0;
    double currDist; // Distance of a point in list
    double minDist = -1;
    list<Item>::iterator iter;
    list<Item>::iterator iterNearestNeighbor;
    vector<neighborVertice> neighborVertices; // Keep all neighbors  
    int numNeighbors = 0; // Number of neighbors

    status = SUCCESS;

    /* Check model */
    if(this->fitted == 0){
        status = METHOD_UNFITTED;
        return;
    }

    if(this->k == -1){
        status = INVALID_METHOD;
        return;
    }

    /* Find initial vertice */
    initialPos = this->hashFunctions->hash(query, status);
    if(status != SUCCESS)
            return;

    /* Find all neighbors of current vertice */
    for(i = 0; i < this->tableSize; i++){

        /* Don't check the same position */
        if(i == initialPos)
            continue;

        neighborVertices.push_back(neighborVertice(hammingDistance(initialPos, i), i));
    } // End for

    /* Sort neighborVertices */
    make_heap(neighborVertices.begin(), neighborVertices.end(),verticesCompare());

    /* Check probes vertices for neighbors */
    for(i = 0; i < this->probes; i++){
   
        /* Check initial pos */
        if(i == 0)
            pos = initialPos;
        /* Extract min pos */
        else
            pos = neighborVertices.front().pos;

        /* Empty vertice */
        if(this->cube[pos].size() == 0)
            continue;

        /* Scan current vertice */
        for(iter = this->cube[pos].begin(); iter != this->cube[pos].end(); iter++){  

            numNeighbors += 1;
            
            /* Find current distance */
            currDist = iter->cosineDist(query, status);
            if(status != SUCCESS)
                return;
            
            /* First neighbor */
            if(flag == 0){
                minDist = currDist;
                iterNearestNeighbor = iter;
    
                found = 1;
                flag = 1;
            }

            /* Keep neighbor */
            else if(minDist > currDist){
                iterNearestNeighbor = iter;
                minDist = currDist;

            }

            /* Found m neighbors */
            if(numNeighbors == m)
                break;
        } // End for - Scan list

        /* Found m neighbors */
        if(numNeighbors == m)
            break;
    } // End for - Probes

    /* Nearest neighbor found */
    if(found == 1){
        nNeighbor = *iterNearestNeighbor;
        if(neighborDistance != NULL)
            *neighborDistance = minDist;
    }
    else{
        nNeighbor.setId("Nearest neighbor not found");
        if(neighborDistance != NULL)
            *neighborDistance = minDist;
    }
}

///////////////
/* Accessors */
///////////////

int hypercubeCosine::getNumberOfPoints(errorCode& status){
    status = SUCCESS;

    if(fitted == 0){
        status = METHOD_UNFITTED;
        return -1;
    }
    else if(this->k == -1){
        status = INVALID_METHOD;
        return -1;
    }
    else
        return this->n;
}

int hypercubeCosine::getDim(errorCode& status){
    status = SUCCESS;

    if(fitted == 0){
        status = METHOD_UNFITTED;
        return -1;
    }
    else if(this->k == -1){
        status = INVALID_METHOD;
        return -1;
    }
    else
        return this->dim;
}

unsigned hypercubeCosine::size(void){
    unsigned result = 0;

    if(fitted == 0){
        return -1;
    }
    
    if(this->k == -1){
        return -1;
    }
    
    result += sizeof(this->tableSize);
    result += sizeof(this->n);
    result += sizeof(this->k);
    result += sizeof(this->dim);
    result += sizeof(this->m);
    result += sizeof(this->m);
    result += sizeof(this->probes);
    result += sizeof(this->fitted);
    
    int i;

    result += this->hashFunctions->size();

    result += sizeof(this->hashFunctions);

    list<Item>::iterator iter;

    for(i = 0; i < this->tableSize; i++){
        for(iter = this->cube[i].begin(); iter!= this->cube[i].end(); iter++){
            result += iter->size();

            result += sizeof(Item);
        } // End for - iter
    } // End for - table size

    result += this->cube.capacity() * sizeof(list<Item>);

    result += sizeof(this->cube);

    return result;

}

/* Print statistics */
void hypercubeCosine::print(void){

    if(this->k == -1)
        cout << "Invalid method\n";
    else{

        cout << "Hypercube cosine statistics\n";
        cout << "Size per table: " << this->tableSize << "\n";
        cout << "Number of sub hash functions(k): " << this->k << "\n";
        cout << "M: " << this->m << "\n";
        cout << "Probes: " << this->probes << "\n";
    }
}

void hypercubeCosine::printHashFunctions(void){

    if(this->k == -1)
        cout << "Invalid method\n";
    else if(this->fitted == 0)
        cout << "Method not fitted\n";
    else{

        cout << "Hash functions hypercube cosine\n";
        this->hashFunctions->print();
    }
}

// Petropoulakis Panagiotis

#include <iostream>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <algorithm>
#include <random>


#include "objects.h"
#include "auxilary.h"
#include "process.h"

double fRand(double fMin, double fMax) {
    double f = (double)rand() / RAND_MAX;
    return fMin + f * (fMax - fMin);
}

bool threshold(struct PHOTON photon1, struct PHOTON photon2) {
    double cosphi = photon1.k.x * photon2.k.x + photon1.k.y * photon2.k.y + photon1.k.z * photon2.k.z;

    double E1E2 = photon1.E * photon2.E;
    double s = E1E2 * (1.0 - cosphi) / (2.0 * 0.5 * 0.5);
    return (s >= 1.0);
}

int check_interact(double randNum, std::vector<std::pair<int, double> > probIJ) {
    double sum = 0.0;
    for(int i = 0; i <= probIJ.size(); i++) {
        sum += probIJ[i].second;
        if(randNum < sum) return probIJ[i].first;
    }
    return -1;
}

double sigmaGG(double sigma, struct PHOTON photon1, struct PHOTON photon2) {
    double cosphi = photon1.k.x * photon2.k.x + photon1.k.y * photon2.k.y + photon1.k.z * photon2.k.z;
    double E1E2 = photon1.E * photon2.E;
    double s = E1E2 * (1.0 - cosphi) / (2.0 * 0.5 * 0.5);

    double beta = sqrt(1.0 - 1.0 / s);
    double fs = (1.0 - pow(beta, 2)) * (-2.0 * beta * (2.0 - pow(beta, 2)) + (3.0 - pow(beta, 4)) * log((1.0 + beta) / (1.0 - beta)));
    // std::cout << 0.000001 * 0.008 / pow(0.01, 3) << "\n";
    return sigma * fs;
}

void inject_photons(int number, double dx, double L, std::vector<struct PHOTON> *photons) {
    std::default_random_engine generator;
    std::lognormal_distribution<double> distribution(4.0,1.0);
    for (int i = 0; i < number; i ++) {
        double kx1 = fRand(-1.0, 1.0);
        double ky1 = 0.0;
        double kz1 = fRand(-1.0, 1.0);
        double kk1 = sqrt(kx1 * kx1 + ky1 * ky1 + kz1 * kz1);
        double E1 = 2.0; //distribution(generator);

        VECTOR temp_k1(kx1 / kk1, ky1 / kk1, kz1 / kk1);
        VECTOR temp_r1(-L / 4.0, 0.0, 0.0);
        temp_r1.x += fRand(0.0, 1.0) * temp_k1.x * dx;
        // temp_r1.y += 3.0 * temp_k1.y * dx;
        temp_r1.z += fRand(0.0, 1.0) * temp_k1.z * dx;
        PHOTON temp_photon1(0, E1, temp_k1, temp_r1);
        photons->push_back(temp_photon1);

        double kx2 = fRand(-1.0, 1.0);
        double ky2 = 0.0;
        double kz2 = fRand(-1.0, 1.0);
        double kk2 = sqrt(kx2 * kx2 + ky2 * ky2 + kz2 * kz2);
        double E2 = 2.0; //distribution(generator);

        VECTOR temp_k2(kx2 / kk2, ky2 / kk2, kz2 / kk2);
        VECTOR temp_r2(L / 4.0, 0.0, 0.0);
        temp_r2.x += fRand(0.0, 2.0) * temp_k2.x * dx;
        // temp_r2.y += 3.0 * temp_k2.y * dx;
        temp_r2.z += fRand(0.0, 2.0) * temp_k2.z * dx;
        PHOTON temp_photon2(1, E2, temp_k2, temp_r2);
        photons->push_back(temp_photon2);

        if (E1 <= 0 || E2 <= 0) {
            std::cout << "Negative energy generated." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

void move_photons(double cdt, double dx, double L, std::vector<struct PHOTON> *photons) {
    for (auto photon = (*photons).begin(); photon != (*photons).end(); ) {
        /*
          moving
        */
        (*photon).r.x += (*photon).k.x * cdt;
        (*photon).r.y += (*photon).k.y * cdt;
        (*photon).r.z += (*photon).k.z * cdt;

        /*
          if photon is out of bound - delete it
        */
        if ((*photon).r.y >= 0.5 * dx ||
            (*photon).r.y < -0.5 * dx) {
                std::cout << "Particle flew out of y-boundary." << std::endl;
                exit(EXIT_FAILURE);
            }
        if ((*photon).r.x >= 0.5 * L ||
            (*photon).r.x < -0.5 * L ||
            (*photon).r.z >= 0.5 * L ||
            (*photon).r.z < -0.5 * L) {
                photon = photons->erase(photon);
        } else {
            ++photon;
        }
    }
}

void interact_photons(double sigma, double cdt, double dx, double L, std::vector<std::pair<int, int> > *CELLS, std::vector<struct PHOTON> *photons, std::vector<std::vector <int> > *grid) {
    int N_CELLS = (int)(L / dx);
    for (int phI = 0; phI < (*photons).size(); ) {
        int cell_x = floor(((*photons)[phI].r.x + L / 2.0) / dx);
        int cell_z = floor(((*photons)[phI].r.z + L / 2.0) / dx);
        int cell_num = cell_x + cell_z * N_CELLS;

        int phStart, phEnd;
        if (cell_num == 0) {
            if((*CELLS)[0].first < 2) {
                phI ++;
                continue; // if there is only one particle in that cell
            }
            phStart = 0;
            phEnd = (*CELLS)[0].first;
        } else {
            if((*CELLS)[cell_num].first - (*CELLS)[cell_num - 1].first < 2) {
                phI ++;
                continue; // if there is only one particle in that cell
            }
            phStart = (*CELLS)[cell_num - 1].first;
            phEnd = (*CELLS)[cell_num].first;
        }
        for(int photonI = phStart; photonI < phEnd; photonI ++) {
            if ((*photons)[photonI].toDelete) continue;

            double tauI = 0.0; // optical depth of the given photon I

            std::vector<std::pair<int, double> > pIJ;
            for(int photonJ = phStart; photonJ < phEnd; photonJ ++) {
                if ((*photons)[photonJ].toDelete || photonI == photonJ) continue;

                if (!threshold((*photons)[photonI], (*photons)[photonJ]) || (*photons)[photonJ].id == (*photons)[photonI].id) continue;

                double tauIJ = sigmaGG(sigma, (*photons)[photonI], (*photons)[photonJ]);// * ;
                // accumulating optical depth
                tauI += tauIJ;
                // storing indexes of all the interaction probabilities for a given photon I and neighboring photons J
                pIJ.push_back({photonJ, tauIJ});
            }
            if (fRand(0.0, 1.0) <= tauI) { // this means that photon I pair produces, then we need to choose with which of the other photons in the same cell
                (*grid)[cell_x][cell_z] ++;
                (*photons)[photonI].toDelete = true;
                for (int j = 0; j < pIJ.size(); j++) { // normalizing tauIJ by tauI
                    pIJ[j].second /= tauI;
                }
                /*
                  choosing with which of the photons to interact
                */
                // if (cell_num == N_CELLS * (N_CELLS / 2) + (N_CELLS / 2)) {
                //     std::cout << "\ttauI = " << tauI << std::endl;
                // }
                double random = fRand(0.0, 1.0);
                int photonJ = check_interact(random, pIJ);
                // std::cout << "interaction! " << pIJ[0].second << " " << pIJ[1].second << std::endl;
                if (photonJ < 0) {
                    std::cout << cell_num << " " << photonI << " " << photonJ << "\n";
                    std::cout << "Photon number error in 'check_interact'\n";
                    exit (EXIT_FAILURE);
                }
                (*photons)[photonJ].toDelete = true;
            }
        }
        phI = phEnd;
    }
    clean_things_up(photons);
}

  // for (int cellPh = 0; cellPh < (*photons).size(); cellPh ++) { // choosing one of the photons
  //   if ((*photons)[cellPh].counted) continue;
  //
  //   int nx = floor(((*photons)[cellPh].r.x + L / 2.0) / dx);
  //   int nz = floor(((*photons)[cellPh].r.z + L / 2.0) / dx);
  //
  //   (*photons)[cellPh].counted = true;
  //
  //   std::vector<int> photonsInCell;
  //   for (int ph = 0; ph < (*photons).size(); ph++) { // looking for the photons in the same cell
  //     struct PHOTON photon = (*photons)[ph];
  //     if (cellPh == ph || photon.counted == true) continue;
  //     /*
  //       find in which cell is the photon
  //     */
  //     int cell_x = floor((photon.r.x + L / 2.0) / dx);
  //     int cell_z = floor((photon.r.z + L / 2.0) / dx);
  //     /*
  //       if cell in which the photon seats is out of bound - error
  //     */
  //     if (cell_x < 0 || cell_x >= N_CELLS ||
  //         cell_z < 0 || cell_z >= N_CELLS) {
  //           std::cout << cell_x << " " << cell_z << "\n";
  //           std::cout << "Cell number error in 'interact_photons'\n";
  //           exit (EXIT_FAILURE);
  //         }
  //     /*
  //       making subarray from all photons of the given cell
  //     */
  //     if (cell_x == nx && cell_z == nz) {
  //       if ((*photons)[ph].counted) {
  //         std::cout << "Weird error detected!\n";
  //         exit (EXIT_FAILURE);
  //       }
  //       (*photons)[ph].counted = true;
  //       photonsInCell.push_back(ph);
  //     }
  //   }
  //   if (photonsInCell.size() <= 1) continue;
    /*
      interaction between photons in a given cell
    */
  //   for (auto photonI = photonsInCell.begin(); photonI != photonsInCell.end(); ) {
  //     if ((*photons)[*photonI].toDelete){
  //       ++photonI;
  //       continue;
  //     }
  //     double tauI = 0.0; // optical depth of the given photon I
  //     std::vector<std::pair<int, double> > pIJ;
  //     for (auto photonJ = photonsInCell.begin(); photonJ != photonsInCell.end(); ) {
  //       if ((*photons)[*photonJ].toDelete) {
  //         ++photonJ;
  //         continue;
  //       }
  //       if (photonI != photonJ) {
  //         if (!threshold((*photons)[*photonI], (*photons)[*photonJ]) || (*photons)[*photonJ].id == (*photons)[*photonI].id) {
  //           // std::cout << (*photons)[*photonJ].id << std::endl;
  //           ++photonJ;
  //           continue;
  //         }
  //         double tauIJ = sigmaGG((*photons)[*photonI], (*photons)[*photonJ]) * cdt / pow(dx, 3);
  //         tauI += tauIJ;
  //         pIJ.push_back({*photonJ, tauIJ});
  //       }
  //       ++photonJ;
  //     }
  //     if (fRand(0.0, 1.0) <= tauI) { // this means that photon I pair produces, then we need to choose with which of the other photons in the same cell
  //       (*grid)[nx][nz] ++;
  //       (*photons)[*photonI].toDelete = true;
  //       for (int j = 0; j < pIJ.size(); j++) { // normalizing tauIJ by tauI
  //         pIJ[j].second /= tauI;
  //       }
  //       /*
  //         choosing with which of the photons to interact
  //       */
  //       double random = fRand(0.0, 1.0);
  //       int photonJ = check_interact(random, pIJ);
  //       // std::cout << "interaction! " << pIJ[0].second << " " << pIJ[1].second << std::endl;
  //       if (photonJ < 0) {
  //         std::cout << *photonI << " " << photonJ << "\n";
  //         std::cout << "Photon number error in 'check_interact'\n";
  //         exit (EXIT_FAILURE);
  //       }
  //       (*photons)[photonJ].toDelete = true;
  //     }
  //     ++photonI;
  //   }
  //   clean_things_up(photons);
  // }

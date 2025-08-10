/* chess.c
   Observações:
    - Peças: P/p = peão, R/r = torre, N/n = cavalo, B/b = bispo, Q/q = rainha, K/k = rei
    - Brancas: maiúsculas; Pretas: minúsculas
    - Jogador humano joga com as brancas por padrão; você pode trocar.
    - Entrada de jogada: "e2e4" ou "e2 e4" (sem aspas). Para promoção, ao mover o peão para última rank
      o programa pedirá qual peça escolher (Q/R/B/N).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define BOARD_SIZE 8

/* Configuráveis */
int AI_DEPTH = 3; /* profundidade do minimax (melhore desempenho vs força) */

/* Tabuleiro: [rank][file] com 0,0 = a8 e 7,7 = h1 para facilitar impressão */
typedef struct {
    char cell[BOARD_SIZE][BOARD_SIZE];
} Board;

/* Representa um movimento */
typedef struct {
    int r1, f1; /* origem: rank, file */
    int r2, f2; /* destino */
    char promotion; /* 'Q','R','B','N' ou '\0' */
} Move;

/* Peça negativa/positiva: brancas positivas (valores positivos), pretas negativas */
int piece_value(char p) {
    switch (toupper(p)) {
        case 'P': return 100;
        case 'N': return 320;
        case 'B': return 330;
        case 'R': return 500;
        case 'Q': return 900;
        case 'K': return 20000;
    }
    return 0;
}

/* Utilitários de cor */
int is_white(char p) { return p && isupper((unsigned char)p); }
int is_black(char p) { return p && islower((unsigned char)p); }
int same_color(char a, char b) {
    if (a == '.' || b == '.') return 0;
    return (is_white(a) && is_white(b)) || (is_black(a) && is_black(b));
}

/* Inicializa o tabuleiro padrão */
void init_board(Board *bd) {
    const char *init[8] = {
        "rnbqkbnr", /* rank 8 */
        "pppppppp", /* rank 7 */
        "........", /* 6 */
        "........", /* 5 */
        "........", /* 4 */
        "........", /* 3 */
        "PPPPPPPP", /* rank 2 */
        "RNBQKBNR"  /* rank 1 */
    };
    for (int r=0;r<8;r++) for (int f=0;f<8;f++) bd->cell[r][f] = init[r][f];
}

/* Imprime o tabuleiro de forma legível */
void print_board(Board *bd) {
    printf("   a b c d e f g h\n");
    for (int r=0;r<8;r++) {
        printf("%d  ", 8 - r);
        for (int f=0;f<8;f++) {
            char c = bd->cell[r][f];
            if (c == '.') printf(". ");
            else printf("%c ", c);
        }
        printf(" %d\n", 8 - r);
    }
    printf("   a b c d e f g h\n");
}

/* Converte notação algébrica simples 'e2' -> r,f (r 0..7,f 0..7) */
int alg_to_coords(const char *s, int *r, int *f) {
    if (!s || strlen(s) < 2) return 0;
    char file = s[0];
    char rank = s[1];
    if (file < 'a' || file > 'h') return 0;
    if (rank < '1' || rank > '8') return 0;
    *f = file - 'a';
    *r = 8 - (rank - '0'); /* rank '1' -> r=7, '8' -> r=0 */
    return 1;
}

/* Copia tabuleiro */
void copy_board(Board *dst, Board *src) {
    memcpy(dst->cell, src->cell, BOARD_SIZE*BOARD_SIZE);
}

/* Verifica limites */
int in_bounds(int r, int f) {
    return r >= 0 && r < 8 && f >= 0 && f < 8;
}

/* Gera movimentos pseudo-legais para uma peça localizada em (r,f) */
int generate_piece_moves(Board *bd, int r, int f, Move *out, int max_out, int white_turn) {
    /* retorna número de movimentos gerados (pode incluir movimentos que deixem rei em cheque; filtragem posterior) */
    char p = bd->cell[r][f];
    if (p == '.' ) return 0;
    int count = 0;
    int dir = white_turn ? 1 : -1; /* para peões: white moves up (towards decreasing r indices?), but we've stored rank 1 at r=7, so white moves r-- */
    /* Important: Our board representation: r=0 is rank8, r=7 is rank1. White is at bottom (r=6 pawns), they move r-1 (up visually).
       So for white: pawn step = -1; for black: pawn step = +1. We'll use pawn_step variable accordingly. */
    int pawn_step = is_white(p) ? -1 : 1;

    /* Pawn */
    if (toupper(p) == 'P') {
        int r1 = r + pawn_step;
        /* single advance */
        if (in_bounds(r1,f) && bd->cell[r1][f] == '.') {
            if (count < max_out) { out[count++] = (Move){r,f,r1,f,'\0'}; }
            /* double advance from starting rank */
            int start_rank = is_white(p) ? 6 : 1;
            int r2 = r + 2*pawn_step;
            if (r == start_rank && in_bounds(r2,f) && bd->cell[r2][f] == '.' ) {
                if (count < max_out) { out[count++] = (Move){r,f,r2,f,'\0'}; }
            }
        }
        /* captures */
        for (int df = -1; df <= 1; df += 2) {
            int rf = r + pawn_step;
            int ff = f + df;
            if (in_bounds(rf,ff) && bd->cell[rf][ff] != '.' && !same_color(p, bd->cell[rf][ff])) {
                if (count < max_out) { out[count++] = (Move){r,f,rf,ff,'\0'}; }
            }
        }
        /* promotion handled when applying move (if reaches last rank) */
        return count;
    }

    /* Knight */
    if (toupper(p) == 'N') {
        int dr[8] = {-2,-2,-1,-1,1,1,2,2};
        int df[8] = {-1,1,-2,2,-2,2,-1,1};
        for (int k=0;k<8;k++){
            int rr=r+dr[k], ff=f+df[k];
            if (!in_bounds(rr,ff)) continue;
            if (bd->cell[rr][ff]=='.' || !same_color(p, bd->cell[rr][ff])) {
                if (count < max_out) out[count++] = (Move){r,f,rr,ff,'\0'};
            }
        }
        return count;
    }

    /* King */
    if (toupper(p) == 'K') {
        for (int dr=-1;dr<=1;dr++) for (int df=-1;df<=1;df++){
            if (dr==0 && df==0) continue;
            int rr=r+dr, ff=f+df;
            if (!in_bounds(rr,ff)) continue;
            if (bd->cell[rr][ff]=='.' || !same_color(p, bd->cell[rr][ff])) {
                if (count < max_out) out[count++] = (Move){r,f,rr,ff,'\0'};
            }
        }
        return count;
    }

    /* Sliding pieces: Rook, Bishop, Queen */
    int rook_dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    int bishop_dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    if (toupper(p) == 'R' || toupper(p) == 'Q') {
        for (int d=0; d<4; d++) {
            int dr = rook_dirs[d][0], df_ = rook_dirs[d][1];
            int rr=r+dr, ff=f+df_;
            while (in_bounds(rr,ff)) {
                if (bd->cell[rr][ff] == '.') {
                    if (count < max_out) out[count++] = (Move){r,f,rr,ff,'\0'};
                } else {
                    if (!same_color(p, bd->cell[rr][ff])) {
                        if (count < max_out) out[count++] = (Move){r,f,rr,ff,'\0'};
                    }
                    break;
                }
                rr += dr; ff += df_;
            }
        }
    }
    if (toupper(p) == 'B' || toupper(p) == 'Q') {
        for (int d=0; d<4; d++) {
            int dr = bishop_dirs[d][0], df_ = bishop_dirs[d][1];
            int rr=r+dr, ff=f+df_;
            while (in_bounds(rr,ff)) {
                if (bd->cell[rr][ff] == '.') {
                    if (count < max_out) out[count++] = (Move){r,f,rr,ff,'\0'};
                } else {
                    if (!same_color(p, bd->cell[rr][ff])) {
                        if (count < max_out) out[count++] = (Move){r,f,rr,ff,'\0'};
                    }
                    break;
                }
                rr += dr; ff += df_;
            }
        }
    }
    return count;
}

/* Gera todos os movimentos legais do jogador (filtra movimentos que deixam o rei em cheque) */
#define MAX_MOVES 256
int generate_legal_moves(Board *bd, Move *out, int white_turn) {
    Move all[MAX_MOVES];
    int alln = 0;
    /* gerar pseudo-legal */
    for (int r=0;r<8;r++) for (int f=0;f<8;f++) {
        char p = bd->cell[r][f];
        if (p == '.') continue;
        if (white_turn && !is_white(p)) continue;
        if (!white_turn && !is_black(p)) continue;
        alln += generate_piece_moves(bd, r, f, all + alln, MAX_MOVES - alln, white_turn);
        if (alln >= MAX_MOVES) break;
    }
    /* filtrar por legalidade (rei não em cheque após o movimento) */
    int count = 0;
    Board tmp;
    for (int i=0;i<alln;i++) {
        copy_board(&tmp, bd);
        Move m = all[i];
        char captured = tmp.cell[m.r2][m.f2];
        char mover = tmp.cell[m.r1][m.f1];
        tmp.cell[m.r1][m.f1] = '.';
        /* promotion: handled if pawn reaches last rank; by default promote to Q */
        if (toupper(mover) == 'P' && (m.r2==0 || m.r2==7)) {
            /* promote to queen by default; legal branch will allow player to choose on actual move */
            tmp.cell[m.r2][m.f2] = is_white(mover) ? 'Q' : 'q';
        } else {
            tmp.cell[m.r2][m.f2] = mover;
        }
        /* check if own king is in check */
        int in_check = 0;
        /* find king */
        int kr=-1,kf=-1;
        char kingChar = white_turn ? 'K' : 'k';
        for (int rr=0; rr<8; rr++) for (int ff=0; ff<8; ff++) {
            if (tmp.cell[rr][ff] == kingChar) { kr = rr; kf = ff; }
        }
        if (kr == -1) {
            /* king missing (captured) => illegal */
            in_check = 1;
        } else {
            /* See if any opponent piece attacks king */
            for (int rr=0; rr<8 && !in_check; rr++) for (int ff=0; ff<8 && !in_check; ff++) {
                char op = tmp.cell[rr][ff];
                if (op == '.' ) continue;
                if (white_turn && is_white(op)) continue;
                if (!white_turn && is_black(op)) continue;
                Move moves_tmp[MAX_MOVES];
                int n = generate_piece_moves(&tmp, rr, ff, moves_tmp, MAX_MOVES, !white_turn);
                for (int mm=0; mm<n; mm++) {
                    if (moves_tmp[mm].r2 == kr && moves_tmp[mm].f2 == kf) { in_check = 1; break; }
                }
            }
        }
        if (!in_check) {
            /* this move is legal */
            if (count < MAX_MOVES) out[count++] = all[i];
        }
    }
    return count;
}

/* Executa um movimento no tabuleiro (assume legal) */
void apply_move(Board *bd, Move m) {
    char mover = bd->cell[m.r1][m.f1];
    bd->cell[m.r1][m.f1] = '.';
    if (toupper(mover) == 'P' && (m.r2 == 0 || m.r2 == 7) && m.promotion != '\0') {
        char prom = m.promotion;
        if (is_black(mover)) prom = tolower(prom);
        bd->cell[m.r2][m.f2] = prom;
    } else if (toupper(mover) == 'P' && (m.r2 == 0 || m.r2 == 7) && m.promotion == '\0') {
        /* default promotion to queen */
        bd->cell[m.r2][m.f2] = is_white(mover) ? 'Q' : 'q';
    } else {
        bd->cell[m.r2][m.f2] = mover;
    }
}

/* Avaliação de material simples: soma valores das peças (brancas positivas, pretas negativas) */
int evaluate_board(Board *bd) {
    int score = 0;
    for (int r=0;r<8;r++) for (int f=0;f<8;f++) {
        char p = bd->cell[r][f];
        if (p == '.') continue;
        int val = piece_value(p);
        if (is_white(p)) score += val;
        else score -= val;
    }
    return score;
}

/* Checa se jogador (white_turn) está em cheque */
int is_in_check(Board *bd, int white_turn) {
    char kingChar = white_turn ? 'K' : 'k';
    int kr=-1,kf=-1;
    for (int r=0;r<8;r++) for (int f=0;f<8;f++) if (bd->cell[r][f] == kingChar) { kr=r; kf=f; }
    if (kr == -1) return 1; /* king missing => treat as check */
    for (int r=0;r<8;r++) for (int f=0;f<8;f++) {
        char p = bd->cell[r][f];
        if (p == '.') continue;
        if (white_turn && is_white(p)) continue;
        if (!white_turn && is_black(p)) continue;
        Move moves[MAX_MOVES];
        int n = generate_piece_moves(bd, r, f, moves, MAX_MOVES, !white_turn);
        for (int i=0;i<n;i++) {
            if (moves[i].r2 == kr && moves[i].f2 == kf) return 1;
        }
    }
    return 0;
}

/* Minimax com poda alfa-beta; retorna avaliação (pontuação orientada para as brancas) */
int minimax(Board *bd, int depth, int alpha, int beta, int maximizingPlayer) {
    /* Depth 0 ou fim de jogo? */
    Move moves[MAX_MOVES];
    int n = generate_legal_moves(bd, moves, maximizingPlayer);
    if (depth == 0 || n == 0) {
        /* if no moves: checkmate or stalemate - determine */
        if (n == 0) {
            if (is_in_check(bd, maximizingPlayer)) {
                /* checkmate: losing large score */
                int mateScore = maximizingPlayer ? -1000000 : 1000000;
                return mateScore;
            } else {
                /* stalemate */
                return 0;
            }
        }
        return evaluate_board(bd);
    }

    if (maximizingPlayer) {
        int maxEval = INT_MIN;
        Board tmp;
        for (int i=0;i<n;i++) {
            copy_board(&tmp, bd);
            apply_move(&tmp, moves[i]);
            int eval = minimax(&tmp, depth-1, alpha, beta, 0);
            if (eval > maxEval) maxEval = eval;
            if (eval > alpha) alpha = eval;
            if (beta <= alpha) break;
        }
        return maxEval;
    } else {
        int minEval = INT_MAX;
        Board tmp;
        for (int i=0;i<n;i++) {
            copy_board(&tmp, bd);
            apply_move(&tmp, moves[i]);
            int eval = minimax(&tmp, depth-1, alpha, beta, 1);
            if (eval < minEval) minEval = eval;
            if (eval < beta) beta = eval;
            if (beta <= alpha) break;
        }
        return minEval;
    }
}

/* Escolhe a melhor jogada para o lado (white_turn) usando minimax */
Move choose_ai_move(Board *bd, int white_turn) {
    Move moves[MAX_MOVES];
    int n = generate_legal_moves(bd, moves, white_turn);
    Move best = {0,0,0,0,'\0'};
    if (n == 0) return best;
    int bestScore = white_turn ? INT_MIN : INT_MAX;
    Board tmp;
    for (int i=0;i<n;i++) {
        copy_board(&tmp, bd);
        apply_move(&tmp, moves[i]);
        int score = minimax(&tmp, AI_DEPTH-1, INT_MIN/2, INT_MAX/2, !white_turn);
        if (white_turn) {
            if (score > bestScore) { bestScore = score; best = moves[i]; }
        } else {
            if (score < bestScore) { bestScore = score; best = moves[i]; }
        }
    }
    return best;
}

/* Converte coords para algébrico e imprime move */
void print_move(Move m) {
    char s[10];
    s[0] = 'a' + m.f1;
    s[1] = '0' + (8 - m.r1);
    s[2] = '\0';
    char t[10];
    t[0] = 'a' + m.f2;
    t[1] = '0' + (8 - m.r2);
    t[2] = '\0';
    if (m.promotion) printf("%s -> %s (promo %c)\n", s, t, m.promotion);
    else printf("%s -> %s\n", s , t);
}

/* Solicita promoção ao usuário se aplicável */
char prompt_promotion() {
    char buf[10];
    while (1) {
        printf("Promover para (Q/R/B/N): ");
        if (!fgets(buf, sizeof(buf), stdin)) exit(0);
        if (strlen(buf) == 0) continue;
        char c = toupper((unsigned char)buf[0]);
        if (c=='Q'||c=='R'||c=='B'||c=='N') return c;
    }
}

/* Lê jogada do usuário no formato e2e4 ou e2 e4 */
int parse_move_input(const char *line, Move *out) {
    char tmp[32];
    int len = strlen(line);
    int idx = 0;
    for (int i=0;i<len;i++){
        if (!isspace((unsigned char)line[i])) tmp[idx++] = line[i];
        if (idx >= 31) break;
    }
    tmp[idx] = '\0';
    if (idx < 4) return 0;
    char from[3], to[3];
    from[0] = tmp[0]; from[1] = tmp[1]; from[2] = '\0';
    to[0] = tmp[2]; to[1] = tmp[3]; to[2] = '\0';
    int r1,f1,r2,f2;
    if (!alg_to_coords(from,&r1,&f1)) return 0;
    if (!alg_to_coords(to,&r2,&f2)) return 0;
    out->r1 = r1; out->f1 = f1; out->r2 = r2; out->f2 = f2; out->promotion = '\0';
    /* if more chars (promotion letter) */
    if (idx >= 5) {
        char prom = toupper((unsigned char)tmp[4]);
        if (prom=='Q'||prom=='R'||prom=='B'||prom=='N') out->promotion = prom;
    }
    return 1;
}

/* Compara dois movimentos (origem/destino/promo) */
int moves_equal(Move a, Move b) {
    return a.r1==b.r1 && a.f1==b.f1 && a.r2==b.r2 && a.f2==b.f2 && a.promotion==b.promotion;
}

/* Main loop */
int main() {
    Board bd;
    init_board(&bd);
    int white_turn = 1; /* humano joga brancas inicialmente */
    char input[64];

    printf("Bem-vindo ao MateCheck (versao simplificada) — Jogador = Brancas\n");
    printf("Formato de entrada: e2e4 ou e2 e4. Para sair, digite 'quit'.\n");
    printf("Nota: sem roque e sem en-passant. Promocao para Q/R/B/N.\n\n");

    while (1) {
        print_board(&bd);
        /* checar fim de jogo */
        Move legalMoves[MAX_MOVES];
        int nmoves = generate_legal_moves(&bd, legalMoves, white_turn);
        if (nmoves == 0) {
            if (is_in_check(&bd, white_turn)) {
                if (white_turn) printf("XEQUE-MATE! Pretas vencem.\n"); else printf("XEQUE-MATE! Brancas vencem.\n");
            } else {
                printf("Empate por stalemate!\n");
            }
            break;
        }

        if (white_turn) {
            /* jogador humano */
            printf("\nSua vez (brancas). Entre sua jogada: ");
            if (!fgets(input, sizeof(input), stdin)) break;
            if (strncmp(input, "quit", 4) == 0) { printf("Saindo...\n"); break; }
            Move m;
            if (!parse_move_input(input, &m)) { printf("Entrada invalida. Use e2e4.\n"); continue; }
            /* find matching legal move (consider promotions) */
            int found = 0;
            for (int i=0;i<nmoves;i++) {
                Move lm = legalMoves[i];
                /* if move is promotion target, allow user to select type */
                if (lm.r1==m.r1 && lm.f1==m.f1 && lm.r2==m.r2 && lm.f2==m.f2) {
                    /* if promotion required but user didn't pass promotion, prompt */
                    char mover = bd.cell[m.r1][m.f1];
                    if (toupper(mover) == 'P' && (m.r2==0 || m.r2==7)) {
                        if (m.promotion == '\0') {
                            char prom = prompt_promotion();
                            m.promotion = is_white(mover) ? prom : tolower(prom); /* store uppercase, apply later */
                        }
                    }
                    /* set promotion on chosen move for matching */
                    lm.promotion = m.promotion;
                    /* accept move */
                    apply_move(&bd, lm);
                    found = 1;
                    break;
                }
            }
            if (!found) { printf("Jogada ilegal. Tente novamente.\n"); continue; }
            white_turn = 0;
        } else {
            /* AI plays as black */
            printf("\nComputador (pretas) pensando...\n");
            Move ai_move = choose_ai_move(&bd, 0); /* black = 0 */
            /* if move is promotion and ai_move.promotion == '\0', default to 'q' */
            if (ai_move.r1==0 && ai_move.f1==0 && ai_move.r2==0 && ai_move.f2==0) {
                /* fallback if no move chosen (shouldn't happen) */
                ai_move = legalMoves[0];
            }
            /* AI chosen move might need promotion field set if pawn reaches last rank */
            char mover = bd.cell[ai_move.r1][ai_move.f1];
            if (toupper(mover) == 'P' && (ai_move.r2==0 || ai_move.r2==7) && ai_move.promotion == '\0') {
                ai_move.promotion = 'q';
            }
            print_move(ai_move);
            apply_move(&bd, ai_move);
            white_turn = 1;
        }
    }
    return 0;
}

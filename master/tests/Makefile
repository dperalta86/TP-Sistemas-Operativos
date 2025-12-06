# tests/Makefile
.PHONY: all clean test unity criterion valgrind helgrind

all: unity criterion

# Unity tests (legacy)
unity:
	@echo "=== Compilando tests Unity ==="
	@$(MAKE) -C unity

# Criterion tests (nuevos)
criterion:
	@echo "=== Compilando tests Criterion ==="
	@$(MAKE) -C criterion

# Ejecutar todos los tests
test: all
	@echo "\n🧪 === Ejecutando Tests Unity ==="
	@$(MAKE) -C unity test || true
	@echo "\n🔬 === Ejecutando Tests Criterion ==="
	@$(MAKE) -C criterion test

# Valgrind en tests Criterion
valgrind: criterion
	@echo "\n🔍 === Ejecutando Valgrind en tests Criterion ==="
	@$(MAKE) -C criterion valgrind

# Helgrind (detección de race conditions)
helgrind: criterion
	@echo "\n🔧 === Ejecutando Helgrind en tests de concurrencia ==="
	@for test in criterion/concurrency/test_*; do \
		if [ -x $$test ]; then \
			echo "Analizando $$test..."; \
			valgrind --tool=helgrind \
			         --log-file=helgrind_$$(basename $$test).log \
			         $$test; \
		fi; \
	done

clean:
	@$(MAKE) -C unity clean
	@$(MAKE) -C criterion clean
	@rm -f *.log
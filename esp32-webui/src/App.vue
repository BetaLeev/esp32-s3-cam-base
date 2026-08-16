<template>
  <router-view />
</template>

<script setup>
</script>

<style lang="scss">
@import '@/styles/main';

* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

html,
body,
#app {
  width: 100%;
  height: 100%;
  font-family: $font-family-base;
}

#app {
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}
</style>
